from flask import Flask, render_template, send_from_directory, request, Response
import requests

app = Flask(__name__)

API_SERVER = 'http://localhost:8080'


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/static/<path:path>')
def serve_static(path):
    return send_from_directory('static', path)


@app.route('/api/<path:path>', methods=['GET', 'POST', 'PUT', 'DELETE', 'PATCH'])
def proxy_api(path):
    """Проксирование всех API-запросов к C++ серверу"""
    url = f'{API_SERVER}/api/{path}'

    # Передаем query string
    if request.query_string:
        url += f'?{request.query_string.decode()}'

    # Определяем headers для проксирования
    headers = {}
    if request.content_type:
        headers['Content-Type'] = request.content_type

    try:
        if request.method == 'GET':
            resp = requests.get(url, headers=headers, timeout=120)
        elif request.method == 'POST':
            if request.content_type and 'multipart/form-data' in request.content_type:
                # Для multipart передаем файлы
                files = []
                for key in request.files:
                    for f in request.files.getlist(key):
                        files.append((key, (f.filename, f.read(), f.content_type)))
                data = {}
                for key in request.form:
                    data[key] = request.form[key]
                resp = requests.post(url, files=files, data=data, timeout=120)
            else:
                resp = requests.post(url, data=request.get_data(), headers=headers, timeout=120)
        elif request.method == 'PUT':
            resp = requests.put(url, data=request.get_data(), headers=headers, timeout=120)
        elif request.method == 'DELETE':
            resp = requests.delete(url, headers=headers, timeout=120)
        elif request.method == 'PATCH':
            resp = requests.patch(url, data=request.get_data(), headers=headers, timeout=120)
        else:
            return Response('Method not allowed', status=405)

        # Возвращаем ответ от C++ сервера
        return Response(
            resp.content,
            status=resp.status_code,
            content_type=resp.headers.get('Content-Type', 'application/json')
        )
    except requests.exceptions.ConnectionError:
        return Response(
            '{"error": "API server unavailable"}',
            status=503,
            content_type='application/json'
        )
    except requests.exceptions.Timeout:
        return Response(
            '{"error": "API request timeout"}',
            status=504,
            content_type='application/json'
        )


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8001, debug=True)
