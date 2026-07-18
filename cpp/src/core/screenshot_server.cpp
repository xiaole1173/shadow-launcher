// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — Screenshot Server (Implementation)

#include "screenshot_server.h"

// ============================================================
// Construction / Destruction
// ============================================================

ScreenshotServer::ScreenshotServer(QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
}

ScreenshotServer::~ScreenshotServer()
{
    stop();
}

// ============================================================
// Public API
// ============================================================

bool ScreenshotServer::start(quint16 port)
{
    if (m_server) {
        m_server->close();
        delete m_server;
    }

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &ScreenshotServer::onNewConnection);

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qWarning() << "[ScreenshotServer] Failed to listen on port" << port
                   << ":" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    qInfo() << "[ScreenshotServer] Listening on http://127.0.0.1:" << port;
    emit serverStarted(port);
    return true;
}

void ScreenshotServer::stop()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
        emit serverStopped();
    }
}

void ScreenshotServer::setWindow(QQuickWindow* window)
{
    m_window = window;
}

// ============================================================
// Connection Handling
// ============================================================

void ScreenshotServer::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &ScreenshotServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void ScreenshotServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    HttpRequest req = parseRequest(data);

    emit requestReceived(req.method, req.path);
    handleRequest(socket, req);
}

// ============================================================
// HTTP Request Parsing
// ============================================================

ScreenshotServer::HttpRequest ScreenshotServer::parseRequest(const QByteArray& raw)
{
    HttpRequest req;
    req.method = "GET";
    req.path = "/";

    int headerEnd = raw.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        req.body = raw;
        return req;
    }

    QByteArray headerPart = raw.left(headerEnd);
    req.body = raw.mid(headerEnd + 4);

    int firstSpace = headerPart.indexOf(' ');
    int secondSpace = headerPart.indexOf(' ', firstSpace + 1);
    if (firstSpace > 0 && secondSpace > firstSpace) {
        req.method = QString::fromLatin1(headerPart.left(firstSpace));
        req.path = QString::fromLatin1(headerPart.mid(firstSpace + 1, secondSpace - firstSpace - 1));
    }

    return req;
}

// ============================================================
// QML Expression Evaluation (shared helper)
// ============================================================

/// Try to evaluate a QML expression. First tries root context,
/// then falls back to the first root QML object (for property access).
static QVariant evalQml(QQmlApplicationEngine* engine, const QString& expr, bool* okOut, QString* errOut)
{
    // Try root context first
    {
        QQmlExpression ctxExpr(engine->rootContext(), nullptr, expr);
        QVariant result = ctxExpr.evaluate();
        if (!ctxExpr.hasError()) {
            if (okOut) *okOut = true;
            return result;
        }
    }

    // Fallback: try against first root QML object
    if (!engine->rootObjects().isEmpty()) {
        QObject* root = engine->rootObjects().first();
        QQmlExpression rootExpr(engine->rootContext(), root, expr);
        QVariant result = rootExpr.evaluate();
        if (!rootExpr.hasError()) {
            if (okOut) *okOut = true;
            return result;
        }
    }

    // Both failed: return context-level error
    {
        QQmlExpression fallback(engine->rootContext(), nullptr, expr);
        fallback.evaluate();
        if (okOut) *okOut = false;
        if (errOut) *errOut = fallback.error().toString();
        return {};
    }
}

// ============================================================
// Request Dispatch
// ============================================================

void ScreenshotServer::handleRequest(QTcpSocket* socket, const HttpRequest& req)
{
    // ---- GET /screenshot ----
    if (req.method == "GET" && req.path == "/screenshot") {
        if (!m_window) {
            sendError(socket, 503, "Window not available");
            return;
        }
        QImage img = m_window->grabWindow();
        if (img.isNull()) {
            sendError(socket, 500, "Failed to capture window");
            return;
        }
        sendPng(socket, img);
        return;
    }

    // ---- POST /eval ----
    if (req.method == "POST" && req.path == "/eval") {
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(req.body, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            sendError(socket, 400, "Invalid JSON: " + parseErr.errorString());
            return;
        }

        QJsonObject obj = doc.object();
        if (!obj.contains("qml")) {
            sendError(socket, 400, "Missing 'qml' field");
            return;
        }

        QString expression = obj["qml"].toString();
        if (expression.isEmpty()) {
            sendError(socket, 400, "Empty expression");
            return;
        }

        bool ok = false;
        QString errorMsg;
        QVariant result = evalQml(m_engine, expression, &ok, &errorMsg);

        if (!ok) {
            qWarning() << "[ScreenshotServer] Eval error:" << errorMsg;
        }

        emit evalRequested(expression, ok);

        QJsonObject response;
        response["ok"] = ok;
        if (ok) {
            response["value"] = QJsonValue::fromVariant(result);
        } else {
            response["error"] = errorMsg;
        }
        sendJson(socket, ok ? 200 : 400, response);
        return;
    }

    // ---- POST /navigate (convenience wrapper) ----
    if (req.method == "POST" && req.path == "/navigate") {
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(req.body, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            sendError(socket, 400, "Invalid JSON: " + parseErr.errorString());
            return;
        }

        QJsonObject obj = doc.object();

        if (obj.contains("qml")) {
            // Re-dispatch to /eval
            HttpRequest evalReq;
            evalReq.method = "POST";
            evalReq.path = "/eval";
            evalReq.body = req.body;
            handleRequest(socket, evalReq);
            return;
        }

        if (obj.contains("page")) {
            int page = obj["page"].toInt(-1);
            int tab = obj.contains("tab") ? obj["tab"].toInt(-1) : -1;

            // Navigate by setting QML root object properties
            QString expr;
            if (tab >= 0) {
                expr = QString("navListIndex = %1; backend.navigateToTab(%1, %2)").arg(page).arg(tab);
            } else {
                expr = QString("navListIndex = %1").arg(page);
            }

            if (expr.isEmpty()) {
                sendError(socket, 400, "Missing 'page' field");
                return;
            }

            bool ok = false;
            QString errorMsg;
            QVariant result = evalQml(m_engine, expr, &ok, &errorMsg);

            emit evalRequested(expr, ok);

            QJsonObject response;
            response["ok"] = ok;
            if (!ok) {
                response["error"] = errorMsg;
            } else {
                response["value"] = QJsonValue::fromVariant(result);
            }
            sendJson(socket, ok ? 200 : 400, response);
            return;
        }

        // Named page fallback: try by name strings
        if (obj.contains("pageName")) {
            QString qml = QString("backend.navigateToPage('%1')").arg(obj["pageName"].toString());
            bool ok = false;
            QString errorMsg;
            QVariant result = evalQml(m_engine, qml, &ok, &errorMsg);
            emit evalRequested(qml, ok);
            QJsonObject response;
            response["ok"] = ok;
            if (!ok) response["error"] = errorMsg;
            else response["value"] = QJsonValue::fromVariant(result);
            sendJson(socket, ok ? 200 : 400, response);
            return;
        }

        sendError(socket, 400, "Expected 'qml' or 'page' field");
        return;
    }

    // ---- Fallback: 404 ----
    sendError(socket, 404, "Not found: " + req.method + " " + req.path);
}

// ============================================================
// Response Helpers
// ============================================================

void ScreenshotServer::sendJson(QTcpSocket* socket, int status, const QJsonObject& obj)
{
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray response = QStringLiteral(
        "HTTP/1.1 %1 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %2\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).arg(status).arg(body.size()).toUtf8();
    response.append(body);
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void ScreenshotServer::sendPng(QTcpSocket* socket, const QImage& img)
{
    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    buffer.close();

    QByteArray response = QStringLiteral(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/png\r\n"
        "Content-Length: %1\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).arg(pngData.size()).toUtf8();
    response.append(pngData);
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void ScreenshotServer::sendError(QTcpSocket* socket, int status, const QString& message)
{
    QJsonObject obj;
    obj["ok"] = false;
    obj["error"] = message;

    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray response = QStringLiteral(
        "HTTP/1.1 %1 Error\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %2\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).arg(status).arg(body.size()).toUtf8();
    response.append(body);
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
