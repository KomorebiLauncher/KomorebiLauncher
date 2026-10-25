#include "McClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>
#include <expected>
#include <utility>

#include "Json.h"
#include "McProtocol.h"
#include "Result.h"

namespace {

/** A status answer is a few kilobytes; anything past this is a broken or hostile length field. */
constexpr int maxResponseBytes = 2 * 1024 * 1024;

Result<uint8_t> readByte(QByteArray& data)
{
    if (data.isEmpty()) {
        return std::unexpected("No more bytes to read");
    }

    const uint8_t byte = data.at(0);
    data.remove(0, 1);
    return byte;
}

// From https://wiki.vg/Protocol#VarInt_and_VarLong
Result<int> readVarInt(QByteArray& data)
{
    int value = 0;
    int position = 0;

    while (position < 32) {
        TRY_INTO(const auto& currentByte, readByte(data))
        value |= (currentByte & McProtocol::varIntValueMask) << position;

        if ((currentByte & McProtocol::varIntContinueBit) == 0) {
            break;
        }

        position += 7;
    }

    if (position >= 32) {
        return std::unexpected("VarInt is too big");
    }

    return value;
}

void writeUInt16(QByteArray& data, const uint16_t value)
{
    QDataStream stream(&data, QIODeviceBase::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << value;
}

}  // namespace

McClient::McClient(QObject* parent, QString domain, QString ip, const uint16_t port)
    : QObject(parent), m_domain(std::move(domain)), m_ip(std::move(ip)), m_port(port)
{}

void McClient::getStatusData()
{
    qDebug() << "Connecting to socket..";

    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        qDebug() << "Connected to socket successfully";
        sendRequest();

        connect(&m_socket, &QTcpSocket::readyRead, this, &McClient::readRawResponse);
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this]() { emitFail("Socket disconnected: " + m_socket.errorString()); });

    m_socket.connectToHost(m_ip, m_port);
}

void McClient::sendRequest()
{
    QByteArray data;
    McProtocol::writeVarInt(data, 0x00);      // packet ID
    McProtocol::writeVarInt(data, 763);       // hardcoded protocol version (763 = 1.20.1)
    McProtocol::writeString(data, m_domain);  // server address
    writeUInt16(data, m_port);                // server port
    McProtocol::writeVarInt(data, 0x01);      // next state
    writePacketToSocket(data);                // send handshake packet

    McProtocol::writeVarInt(data, 0x00);  // packet ID
    writePacketToSocket(data);            // send status packet
}

void McClient::readRawResponse()
{
    if (m_finished || m_responseReadState == ResponseReadState::Finished) {
        return;
    }

    m_resp.append(m_socket.readAll());
    if (m_responseReadState == ResponseReadState::Waiting && m_resp.size() >= 5) {
        auto res = readVarInt(m_resp);
        if (!res) {
            emitFail(res.error());
            return;
        }
        m_wantedRespLength = *res;
        // VarInts are decoded as signed, and nothing about the protocol guarantees the field is
        // sane. A negative or absurd length would otherwise parse an empty buffer or grow without
        // bound.
        if (m_wantedRespLength < 0 || m_wantedRespLength > maxResponseBytes) {
            emitFail(tr("The server announced an impossible status packet (%1 bytes)").arg(m_wantedRespLength));
            return;
        }
        m_responseReadState = ResponseReadState::GotLength;
    }

    if (m_responseReadState == ResponseReadState::GotLength && m_resp.size() >= m_wantedRespLength) {
        if (m_resp.size() > m_wantedRespLength) {
            qDebug().nospace() << "Warning: Packet length doesn't match actual packet size (" << m_wantedRespLength << " expected vs "
                               << m_resp.size() << " received)";
        }
        parseResponse();
        m_responseReadState = ResponseReadState::Finished;
    }
}

void McClient::parseResponse()
{
    qDebug() << "Received response successfully";

    const auto packetID = readVarInt(m_resp);
    if (!packetID) {
        emitFail(packetID.error());
        return;
    }
    if (*packetID != 0x00) {
        emitFail(QString("Packet ID doesn't match expected value (0x00 vs 0x%1)").arg(*packetID, 0, 16));
        return;
    }

    {
        auto result = readVarInt(m_resp);  // json length
        if (!result) {
            emitFail(result.error());
            return;
        }
    }

    // 'resp' should now be the JSON string
    QJsonParseError parseError;
    const QJsonDocument doc = Json::parseUntilGarbage(m_resp, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse JSON:" << parseError.errorString();
        emitFail(parseError.errorString());
        return;
    }
    emitSucceed(doc.object());
}

void McClient::writePacketToSocket(QByteArray& data)
{
    // we prefix the packet with its length
    QByteArray dataWithSize;
    McProtocol::writeVarInt(dataWithSize, static_cast<int>(data.size()));
    dataWithSize.append(data);

    // write it to the socket
    m_socket.write(dataWithSize);
    m_socket.flush();

    data.clear();
}

void McClient::emitFail(const QString& error)
{
    if (m_finished)
        return;
    m_finished = true;
    // Stop reading: the answer is not going to be used, and a second error must not be reported.
    m_socket.abort();
    qDebug() << "Minecraft server ping for status error:" << error;
    emit failed(error);
    emit finished();
}

void McClient::emitSucceed(QJsonObject data)
{
    if (m_finished)
        return;
    m_finished = true;
    emit succeeded(std::move(data));
    emit finished();
}
