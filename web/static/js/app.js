// API базовый URL (C++ сервер)
const API_BASE = 'http://localhost:8080';

// Состояние приложения
let graphData = { nodes: [], links: [] };
let selectedNode = null;
let simulation = null;

// Цветовая палитра для предметов
const subjectColors = d3.scaleOrdinal(d3.schemeCategory10);

// DOM элементы
const svg = d3.select('#graph');
const loading = document.getElementById('loading');
const nodeInfo = document.getElementById('node-info');
const nodeActions = document.getElementById('node-actions');
const nodeCountEl = document.getElementById('node-count');
const linkCountEl = document.getElementById('link-count');

// Инициализация
document.addEventListener('DOMContentLoaded', () => {
    initGraph();
    loadNodes();
    setupEventListeners();
});

// Настройка обработчиков событий
function setupEventListeners() {
    // Форма добавления узла
    document.getElementById('add-node-form').addEventListener('submit', handleAddNode);

    // Кнопки перерасчета связей
    document.getElementById('btn-cluster-embeddings').addEventListener('click', () => {
        recalculateLinks('embeddings');
    });
    document.getElementById('btn-cluster-tags').addEventListener('click', () => {
        recalculateLinks('tags');
    });

    // Кнопка удаления узла
    document.getElementById('btn-delete-node').addEventListener('click', handleDeleteNode);
}

// Инициализация графа D3.js
function initGraph() {
    const container = document.querySelector('.graph-container');
    const width = container.clientWidth;
    const height = container.clientHeight;

    svg.attr('width', width).attr('height', height);

    // Группа для зума
    const g = svg.append('g').attr('class', 'graph-group');

    // Зум
    const zoom = d3.zoom()
        .scaleExtent([0.1, 4])
        .on('zoom', (event) => {
            g.attr('transform', event.transform);
        });

    svg.call(zoom);

    // Группы для связей и узлов
    g.append('g').attr('class', 'links');
    g.append('g').attr('class', 'nodes');

    // Адаптация при изменении размера окна
    window.addEventListener('resize', () => {
        const newWidth = container.clientWidth;
        const newHeight = container.clientHeight;
        svg.attr('width', newWidth).attr('height', newHeight);
        if (simulation) {
            simulation.force('center', d3.forceCenter(newWidth / 2, newHeight / 2));
            simulation.alpha(0.3).restart();
        }
    });
}

// Загрузка узлов из API
async function loadNodes() {
    showLoading(true);
    try {
        const response = await fetch(`${API_BASE}/api/nodes`);
        if (!response.ok) throw new Error('Failed to load nodes');

        const data = await response.json();
        const nodes = data.nodes || [];

        // Преобразование данных для D3
        graphData.nodes = nodes.map(node => ({
            id: node.id,
            title: node.title,
            author: node.author,
            subject: node.subject,
            course: node.course,
            description: node.description,
            date: node.date,
            tags: node.tags || [],
            linkedNodes: node.LinkedNodes || []
        }));

        // Построение связей
        graphData.links = [];
        const nodeIds = new Set(graphData.nodes.map(n => n.id));

        graphData.nodes.forEach(node => {
            node.linkedNodes.forEach(targetId => {
                if (nodeIds.has(targetId) && node.id < targetId) {
                    graphData.links.push({
                        source: node.id,
                        target: targetId
                    });
                }
            });
        });

        updateGraph();
        updateStats();
        notify('Граф загружен', 'success');
    } catch (error) {
        console.error('Error loading nodes:', error);
        notify('Ошибка загрузки данных', 'error');
    } finally {
        showLoading(false);
    }
}

// Обновление визуализации графа
function updateGraph() {
    const container = document.querySelector('.graph-container');
    const width = container.clientWidth;
    const height = container.clientHeight;

    const g = svg.select('.graph-group');

    // Симуляция силы
    simulation = d3.forceSimulation(graphData.nodes)
        .force('link', d3.forceLink(graphData.links).id(d => d.id).distance(100))
        .force('charge', d3.forceManyBody().strength(-300))
        .force('center', d3.forceCenter(width / 2, height / 2))
        .force('collision', d3.forceCollide().radius(40));

    // Связи
    const linkSelection = g.select('.links')
        .selectAll('.link')
        .data(graphData.links, d => `${d.source.id || d.source}-${d.target.id || d.target}`);

    linkSelection.exit().remove();

    const linkEnter = linkSelection.enter()
        .append('line')
        .attr('class', 'link');

    const links = linkEnter.merge(linkSelection);

    // Узлы
    const nodeSelection = g.select('.nodes')
        .selectAll('.node')
        .data(graphData.nodes, d => d.id);

    nodeSelection.exit().remove();

    const nodeEnter = nodeSelection.enter()
        .append('g')
        .attr('class', 'node')
        .call(d3.drag()
            .on('start', dragStarted)
            .on('drag', dragged)
            .on('end', dragEnded));

    nodeEnter.append('circle')
        .attr('r', 20)
        .attr('fill', d => subjectColors(d.subject));

    nodeEnter.append('text')
        .attr('dy', 35)
        .attr('text-anchor', 'middle')
        .text(d => truncate(d.title, 15));

    const nodes = nodeEnter.merge(nodeSelection);

    // Обновление цветов
    nodes.select('circle')
        .attr('fill', d => subjectColors(d.subject));

    nodes.select('text')
        .text(d => truncate(d.title, 15));

    // Обработчик клика на узел
    nodes.on('click', (event, d) => {
        event.stopPropagation();
        selectNode(d);
    });

    // Подсветка связей при наведении
    nodes.on('mouseenter', (event, d) => {
        highlightConnections(d, true);
    }).on('mouseleave', (event, d) => {
        highlightConnections(d, false);
    });

    // Клик на пустое место — сбросить выделение
    svg.on('click', () => {
        deselectNode();
    });

    // Обновление позиций при симуляции
    simulation.on('tick', () => {
        links
            .attr('x1', d => d.source.x)
            .attr('y1', d => d.source.y)
            .attr('x2', d => d.target.x)
            .attr('y2', d => d.target.y);

        nodes.attr('transform', d => `translate(${d.x},${d.y})`);
    });
}

// Подсветка связей узла
function highlightConnections(node, highlight) {
    const g = svg.select('.graph-group');

    g.selectAll('.link')
        .classed('highlighted', d => {
            if (!highlight) return false;
            const sourceId = d.source.id || d.source;
            const targetId = d.target.id || d.target;
            return sourceId === node.id || targetId === node.id;
        });
}

// Выбор узла
function selectNode(node) {
    selectedNode = node;

    // Обновить визуальное выделение
    svg.selectAll('.node').classed('selected', d => d.id === node.id);

    // Показать информацию
    showNodeInfo(node);
    nodeActions.classList.remove('hidden');
}

// Снять выделение
function deselectNode() {
    selectedNode = null;
    svg.selectAll('.node').classed('selected', false);
    nodeInfo.innerHTML = '<p class="placeholder">Выберите узел на графе</p>';
    nodeActions.classList.add('hidden');
}

// Показать информацию об узле
function showNodeInfo(node) {
    let html = `
        <div class="info-row">
            <div class="info-label">ID</div>
            <div class="info-value">${node.id}</div>
        </div>
        <div class="info-row">
            <div class="info-label">Название</div>
            <div class="info-value">${escapeHtml(node.title)}</div>
        </div>
        <div class="info-row">
            <div class="info-label">Автор</div>
            <div class="info-value">${escapeHtml(node.author)}</div>
        </div>
        <div class="info-row">
            <div class="info-label">Предмет</div>
            <div class="info-value">${escapeHtml(node.subject)}</div>
        </div>
    `;

    if (node.course) {
        html += `
            <div class="info-row">
                <div class="info-label">Курс</div>
                <div class="info-value">${node.course}</div>
            </div>
        `;
    }

    if (node.description) {
        html += `
            <div class="info-row">
                <div class="info-label">Описание</div>
                <div class="info-value">${escapeHtml(node.description)}</div>
            </div>
        `;
    }

    if (node.tags && node.tags.length > 0) {
        html += `
            <div class="info-row">
                <div class="info-label">Теги</div>
                <div class="tags">
                    ${node.tags.map(tag => `<span class="tag">${escapeHtml(tag)}</span>`).join('')}
                </div>
            </div>
        `;
    }

    if (node.linkedNodes && node.linkedNodes.length > 0) {
        const linkedTitles = node.linkedNodes.map(id => {
            const linked = graphData.nodes.find(n => n.id === id);
            return linked ? `<div class="linked-node" data-id="${id}">${escapeHtml(linked.title)}</div>` : '';
        }).filter(Boolean).join('');

        html += `
            <div class="info-row">
                <div class="info-label">Связанные узлы</div>
                <div class="linked-nodes">${linkedTitles}</div>
            </div>
        `;
    }

    nodeInfo.innerHTML = html;

    // Обработчики кликов на связанные узлы
    nodeInfo.querySelectorAll('.linked-node').forEach(el => {
        el.addEventListener('click', () => {
            const id = parseInt(el.dataset.id);
            const targetNode = graphData.nodes.find(n => n.id === id);
            if (targetNode) {
                selectNode(targetNode);
                centerOnNode(targetNode);
            }
        });
    });
}

// Центрировать вид на узле
function centerOnNode(node) {
    const container = document.querySelector('.graph-container');
    const width = container.clientWidth;
    const height = container.clientHeight;

    const transform = d3.zoomIdentity
        .translate(width / 2 - node.x, height / 2 - node.y);

    svg.transition()
        .duration(500)
        .call(d3.zoom().transform, transform);
}

// Drag функции
function dragStarted(event, d) {
    if (!event.active) simulation.alphaTarget(0.3).restart();
    d.fx = d.x;
    d.fy = d.y;
}

function dragged(event, d) {
    d.fx = event.x;
    d.fy = event.y;
}

function dragEnded(event, d) {
    if (!event.active) simulation.alphaTarget(0);
    d.fx = null;
    d.fy = null;
}

// Добавление узла
async function handleAddNode(event) {
    event.preventDefault();

    const form = event.target;
    const formData = new FormData(form);

    const nodeData = {
        title: formData.get('title'),
        author: formData.get('author'),
        subject: formData.get('subject')
    };

    const course = formData.get('course');
    if (course) nodeData.course = parseInt(course);

    const description = formData.get('description');
    if (description) nodeData.description = description;

    const tagsStr = formData.get('tags');
    if (tagsStr) {
        nodeData.tags = tagsStr.split(',').map(t => t.trim()).filter(Boolean);
    }

    try {
        const response = await fetch(`${API_BASE}/api/nodes`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(nodeData)
        });

        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to create node');
        }

        const result = await response.json();
        notify(`Узел создан (ID: ${result.id})`, 'success');

        form.reset();
        await loadNodes();
    } catch (error) {
        console.error('Error creating node:', error);
        notify(`Ошибка: ${error.message}`, 'error');
    }
}

// Удаление узла
async function handleDeleteNode() {
    if (!selectedNode) return;

    if (!confirm(`Удалить узел "${selectedNode.title}"?`)) return;

    try {
        const response = await fetch(`${API_BASE}/api/nodes/${selectedNode.id}`, {
            method: 'DELETE'
        });

        if (!response.ok) {
            throw new Error('Failed to delete node');
        }

        notify(`Узел удален`, 'success');
        deselectNode();
        await loadNodes();
    } catch (error) {
        console.error('Error deleting node:', error);
        notify(`Ошибка удаления: ${error.message}`, 'error');
    }
}

// Перерасчет связей
async function recalculateLinks(method) {
    const endpoint = method === 'embeddings' ? '/api/cluster' : '/api/tags/link-all';
    const methodName = method === 'embeddings' ? 'эмбеддингам' : 'тегам';

    showLoading(true);
    notify(`Перерасчет по ${methodName}...`, 'info');

    try {
        const response = await fetch(`${API_BASE}${endpoint}`, {
            method: 'POST'
        });

        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to recalculate');
        }

        const result = await response.json();
        notify(`Связи пересчитаны`, 'success');

        await loadNodes();
    } catch (error) {
        console.error('Error recalculating links:', error);
        notify(`Ошибка: ${error.message}`, 'error');
    } finally {
        showLoading(false);
    }
}

// Обновить статистику
function updateStats() {
    nodeCountEl.textContent = graphData.nodes.length;
    linkCountEl.textContent = graphData.links.length;
}

// Показать/скрыть индикатор загрузки
function showLoading(show) {
    loading.classList.toggle('hidden', !show);
}

// Уведомления
function notify(message, type = 'info') {
    const container = document.getElementById('notifications');
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    container.appendChild(notification);

    setTimeout(() => {
        notification.remove();
    }, 3000);
}

// Утилиты
function truncate(str, maxLength) {
    if (!str) return '';
    return str.length > maxLength ? str.substring(0, maxLength) + '...' : str;
}

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}
