/*
MIT License

Copyright (c) 2019 Shen Li & Co-Operators

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "isotp.h"

Isotp::Isotp(QObject *parent) : QObject(parent)
{
    millis = new QDateTime();
}

Isotp::~Isotp()
{
    delete sendbuf;
    delete recvbuf;
    delete link;
    pollTimer->stop();
    pollTimer->deleteLater();
}

void Isotp::send(QByteArray payload)
{
    return send_with_id(link->send_arbitration_id, payload);
}

void Isotp::send_with_id(quint32 id, QByteArray payload)
{
    int ret;

    if (link == 0x0) {
        qDebug("Link is null!");
        return;
    }

    if (payload.size() > link->send_buf_size) {
        qDebug("Message size too large. Increase ISO_TP_MAX_MESSAGE_SIZE to set a larger buffer\n");
        char message[128];
        sprintf(&message[0], "Attempted to send %d bytes; max size is %d!\n", payload.size(), link->send_buf_size);
        return;
    }

    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status) {
        qDebug("Abort previous message, transmission in progress.\n");
        return;
    }

    /* copy into local buffer */
    link->send_size = payload.size();
    link->send_offset = 0;
    (void) ::memcpy(link->send_buffer, payload.data_ptr()->data(), payload.size());

    if (link->send_size < 8) {
        /* send single frame */
        isotp_send_single_frame(id);
    } else {
        /* send multi-frame */
        ret = isotp_send_first_frame(id);

        /* init multi-frame control flags */
        if (ISOTP_RET_OK == ret) {
            link->send_bs_remain = 0;
            link->send_st_min = 0;
            link->send_wtf_count = 0;
            link->send_timer_st = millis->toMSecsSinceEpoch();
            link->send_timer_bs = millis->toMSecsSinceEpoch() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            link->send_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            link->send_status = ISOTP_SEND_STATUS_INPROGRESS;
        }
    }
}

quint8 Isotp::isotp_ms_to_st_min(quint8 ms)
{
    quint8 st_min;

    st_min = ms;
    if (st_min > 0x7F) {
        st_min = 0x7F;
    }

    return st_min;
}

quint8 Isotp::isotp_st_min_to_ms(quint8 st_min)
{
    quint8 ms;

    if (st_min >= 0xF1 && st_min <= 0xF9) {
        ms = 1;
    } else if (st_min <= 0x7F) {
        ms = st_min;
    } else {
        ms = 0;
    }

    return ms;
}

int Isotp::isotp_send_flow_control(quint8 flow_status, quint8 block_size, quint8 st_min_ms)
{
    IsoTpCanMessage message;

    /* setup message  */
    message.as.flow_control.type = ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME;
    message.as.flow_control.FS = flow_status;
    message.as.flow_control.BS = block_size;
    message.as.flow_control.STmin = isotp_ms_to_st_min(st_min_ms);

    QCanBusFrame frame;
    QByteArray payload;
    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void) memset(message.as.flow_control.reserve, 0, sizeof(message.as.flow_control.reserve));
    frame.setFrameId(link->send_arbitration_id);
    for (size_t i = 0; i < sizeof(message); i++) {
        payload.append(message.as.data_array.ptr[i]);
    }
    frame.setPayload(payload);
    emit send_can(frame);

#else
    frame.setFrameId(link->send_arbitration_id);
    payload.append(message.as.data_array.ptr[0]);
    payload.append(message.as.data_array.ptr[1]);
    payload.append(message.as.data_array.ptr[2]);
    frame.setPayload(payload);
    emit send_can(frame);
#endif

    return ISOTP_RET_OK;
}

int Isotp::isotp_send_single_frame(quint32 id)
{
    IsoTpCanMessage message;


    /* multi frame message length must greater than 7  */
    assert(link->send_size <= 7);

    /* setup message  */
    message.as.single_frame.type = ISOTP_PCI_TYPE_SINGLE;
    message.as.single_frame.SF_DL = (quint8) link->send_size;
    (void) ::memcpy(message.as.single_frame.data, link->send_buffer, link->send_size);

    QByteArray payload;
    QCanBusFrame frame;
    /* send message */
#ifdef ISO_TP_FRAME_PADDING
    (void) memset(message.as.single_frame.data + link->send_size, 0, sizeof(message.as.single_frame.data) - link->send_size);
    frame.setFrameId(id);
    for (size_t i = 0; i < sizeof(message); i++) {
        payload.append(message.as.data_array.ptr[i]);
    }
    frame.setPayload(payload);
    emit send_can(frame);

#else

    frame.setFrameId(id);

    for (size_t i = 0; i < link->send_size + 1; i++) {
        payload.append(message.as.data_array.ptr[i]);
    }
    frame.setPayload(payload);
    emit send_can(frame);

#endif

    return ISOTP_RET_OK;
}

int Isotp::isotp_send_first_frame(quint32 id)
{
    IsoTpCanMessage message;

    /* multi frame message length must greater than 7  */
    assert(link->send_size > 7);

    /* setup message  */
    message.as.first_frame.type = ISOTP_PCI_TYPE_FIRST_FRAME;
    message.as.first_frame.FF_DL_low = (uint8_t) link->send_size;
    message.as.first_frame.FF_DL_high = (uint8_t) (0x0F & (link->send_size >> 8));
    (void) memcpy(message.as.first_frame.data, link->send_buffer, sizeof(message.as.first_frame.data));

    /* send message */
    QCanBusFrame frame;
    QByteArray payload;
    frame.setFrameId(id);
    for (size_t i = 0; i < sizeof(message); i++) {
        payload.append(message.as.data_array.ptr[i]);
    }
    frame.setPayload(payload);
    emit send_can(frame);

    link->send_offset += sizeof(message.as.first_frame.data);
    link->send_sn = 1;

    return ISOTP_RET_OK;
}

int Isotp::isotp_send_consecutive_frame()
{
    IsoTpCanMessage message;
    quint16 data_length;


    /* multi frame message length must greater than 7  */
    assert(link->send_size > 7);

    /* setup message  */
    message.as.consecutive_frame.type = TSOTP_PCI_TYPE_CONSECUTIVE_FRAME;
    message.as.consecutive_frame.SN = link->send_sn;
    data_length = link->send_size - link->send_offset;
    if (data_length > sizeof(message.as.consecutive_frame.data)) {
        data_length = sizeof(message.as.consecutive_frame.data);
    }
    (void) ::memcpy(message.as.consecutive_frame.data, link->send_buffer + link->send_offset, data_length);

    QCanBusFrame frame;
    QByteArray payload;
    /* send message */
#ifdef ISO_TP_FRAME_PADDING

    (void) memset(message.as.consecutive_frame.data + data_length, 0, sizeof(message.as.consecutive_frame.data) - data_length);

    frame.setFrameId(link->send_arbitration_id);
    for (size_t i = 0; i < sizeof(message); i++) {
        payload.append(message.as.data_array.ptr[i]);
    }
    frame.setPayload(payload);
    emit send_can(frame);

#else

    frame.setFrameId(link->send_arbitration_id);
    for (size_t i = 0; i < data_length + 1; i++) {
        payload.append(message.as.data_array.ptr[i]);
    }
    frame.setPayload(payload);
    emit send_can(frame);

#endif

    link->send_offset += data_length;
    if (++(link->send_sn) > 0x0F) {
        link->send_sn = 0;
    }

    return ISOTP_RET_OK;
}

int Isotp::isotp_receive_single_frame(IsoTpCanMessage &message, quint8 len)
{
    /* check data length */
    if ((0 == message.as.single_frame.SF_DL) || (message.as.single_frame.SF_DL > (len - 1))) {
        qDebug("Single-frame length too small.");
        return ISOTP_RET_LENGTH;
    }

    /* copying data */
    (void) ::memcpy(link->receive_buffer, message.as.single_frame.data, message.as.single_frame.SF_DL);
    link->receive_size = message.as.single_frame.SF_DL;

    return ISOTP_RET_OK;
}

int Isotp::isotp_receive_first_frame(IsoTpCanMessage &message, quint8 len)
{
    uint16_t payload_length;

    if (8 != len) {
        qDebug("First frame should be 8 bytes in length.");
        return ISOTP_RET_LENGTH;
    }

    /* check data length */
    payload_length = message.as.first_frame.FF_DL_high;
    payload_length = (payload_length << 8) + message.as.first_frame.FF_DL_low;

    /* should not use multiple frame transmition */
    if (payload_length <= 7) {
        qDebug("Should not use multiple frame transmission.");
        return ISOTP_RET_LENGTH;
    }

    if (payload_length > link->receive_buf_size) {
        qDebug("Multi-frame response too large for receiving buffer.");
        return ISOTP_RET_OVERFLOW;
    }

    /* copying data */
    (void) ::memcpy(link->receive_buffer, message.as.first_frame.data, sizeof(message.as.first_frame.data));
    link->receive_size = payload_length;
    link->receive_offset = sizeof(message.as.first_frame.data);
    link->receive_sn = 1;

    return ISOTP_RET_OK;
}

int Isotp::isotp_receive_consecutive_frame(IsoTpCanMessage &message, quint8 len)
{
    uint16_t remaining_bytes;

    /* check sn */
    if (link->receive_sn != message.as.consecutive_frame.SN) {
        return ISOTP_RET_WRONG_SN;
    }

    /* check data length */
    remaining_bytes = link->receive_size - link->receive_offset;
    if (remaining_bytes > sizeof(message.as.consecutive_frame.data)) {
        remaining_bytes = sizeof(message.as.consecutive_frame.data);
    }
    if (remaining_bytes > len - 1) {
        qDebug("Consecutive frame too short.");
        return ISOTP_RET_LENGTH;
    }

    /* copying data */
    (void) ::memcpy(link->receive_buffer + link->receive_offset, message.as.consecutive_frame.data, remaining_bytes);

    link->receive_offset += remaining_bytes;
    if (++(link->receive_sn) > 0x0F) {
        link->receive_sn = 0;
    }

    return ISOTP_RET_OK;
}

int Isotp::isotp_receive_flow_control_frame(IsoTpCanMessage &message, quint8 len)
{
    Q_UNUSED(message);
    /* check message length */
    if (len < 3) {
        qDebug("Flow control frame too short.");
        return ISOTP_RET_LENGTH;
    }

    return ISOTP_RET_OK;
}

int Isotp::receive(QByteArray &payload)
{
    quint16 copylen;

    if (ISOTP_RECEIVE_STATUS_FULL != link->receive_status) {
        return ISOTP_RET_NO_DATA;
    }

    copylen = link->receive_size;

    for (size_t i = 0; i < copylen; i++) {
        payload.append(link->receive_buffer[i]);
    }

    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;

    return ISOTP_RET_OK;
}

void Isotp::init_link(quint32 sendId, quint16 sendBufSize, quint16 recvBufSize, quint16 pollInterval)
{
    pollTimer = new QTimer(this);
    pollTimer->setInterval(pollInterval);
    QObject::connect(pollTimer, &QTimer::timeout, this, &Isotp::on_timeout);

    sendbuf = new quint8[sendBufSize];
    recvbuf = new quint8[recvBufSize];
    link = new IsoTpLink;

    ::memset(link, 0, sizeof(IsoTpLink));
    link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
    link->send_status = ISOTP_SEND_STATUS_IDLE;
    link->send_arbitration_id = sendId;
    link->send_buffer = sendbuf;
    link->send_buf_size = sendBufSize;
    link->receive_buffer = recvbuf;
    link->receive_buf_size = recvBufSize;

    pollTimer->start();
}

void Isotp::poll()
{
    int ret;

    /* only polling when operation in progress */
    if (ISOTP_SEND_STATUS_INPROGRESS == link->send_status) {

        /* continue send data */
        if (/* send data if bs_remain is invalid or bs_remain large than zero */
        (ISOTP_INVALID_BS == link->send_bs_remain || link->send_bs_remain > 0) &&
        /* and if st_min is zero or go beyond interval time */
        (0 == link->send_st_min || (0 != link->send_st_min && IsoTpTimeAfter(millis->toMSecsSinceEpoch(), link->send_timer_st)))) {

            ret = isotp_send_consecutive_frame();
            if (ISOTP_RET_OK == ret) {
                if (ISOTP_INVALID_BS != link->send_bs_remain) {
                    link->send_bs_remain -= 1;
                }
                link->send_timer_bs = millis->toMSecsSinceEpoch() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
                link->send_timer_st = millis->toMSecsSinceEpoch() + link->send_st_min;

                /* check if send finish */
                if (link->send_offset >= link->send_size) {
                    link->send_status = ISOTP_SEND_STATUS_IDLE;
                }
            } else {
                link->send_status = ISOTP_SEND_STATUS_ERROR;
            }
        }

        /* check timeout */
        if (IsoTpTimeAfter(millis->toMSecsSinceEpoch(), link->send_timer_bs)) {
            link->send_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_BS;
            link->send_status = ISOTP_SEND_STATUS_ERROR;
        }
    }

    /* only polling when operation in progress */
    if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {

        /* check timeout */
        if (IsoTpTimeAfter(millis->toMSecsSinceEpoch(), link->receive_timer_cr)) {
            link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_TIMEOUT_CR;
            link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
        }
    }
}

void Isotp::on_timeout()
{
    poll();
    QByteArray payload;
    int ret = receive(payload);
    if (ISOTP_RET_OK == ret) {
        emit new_message(payload);
    }
}

void Isotp::on_can_message(QCanBusFrame frame)
{
    IsoTpCanMessage message;
    int ret;

    if (frame.payload().length() < 2 || frame.payload().length() > 8) {
        return;
    }

    ::memcpy(message.as.data_array.ptr, frame.payload().data_ptr()->data(), frame.payload().length());
    ::memset(message.as.data_array.ptr + frame.payload().length(), 0, sizeof(message.as.data_array.ptr) - frame.payload().length());

    switch (message.as.common.type) {
        case ISOTP_PCI_TYPE_SINGLE: {
            /* update protocol result */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            } else {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }

            /* handle message */
            ret = isotp_receive_single_frame(message, frame.payload().length());

            if (ISOTP_RET_OK == ret) {
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
            }
            break;
        }
        case ISOTP_PCI_TYPE_FIRST_FRAME: {
            /* update protocol result */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS == link->receive_status) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
            } else {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_OK;
            }

            /* handle message */
            ret = isotp_receive_first_frame(message, frame.payload().length());

            /* if overflow happened */
            if (ISOTP_RET_OVERFLOW == ret) {
                /* update protocol result */
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                /* send error message */
                isotp_send_flow_control(PCI_FLOW_STATUS_OVERFLOW, 0, 0);
                break;
            }

            /* if receive successful */
            if (ISOTP_RET_OK == ret) {
                /* change status */
                link->receive_status = ISOTP_RECEIVE_STATUS_INPROGRESS;
                /* send fc frame */
                link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                isotp_send_flow_control(PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                /* refresh timer cs */
                link->receive_timer_cr = millis->toMSecsSinceEpoch() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;
            }

            break;
        }
        case TSOTP_PCI_TYPE_CONSECUTIVE_FRAME: {
            /* check if in receiving status */
            if (ISOTP_RECEIVE_STATUS_INPROGRESS != link->receive_status) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_UNEXP_PDU;
                break;
            }

            /* handle message */
            ret = isotp_receive_consecutive_frame(message, frame.payload().length());

            /* if wrong sn */
            if (ISOTP_RET_WRONG_SN == ret) {
                link->receive_protocol_result = ISOTP_PROTOCOL_RESULT_WRONG_SN;
                link->receive_status = ISOTP_RECEIVE_STATUS_IDLE;
                break;
            }

            /* if success */
            if (ISOTP_RET_OK == ret) {
                /* refresh timer cs */
                link->receive_timer_cr = millis->toMSecsSinceEpoch() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

                /* receive finished */
                if (link->receive_offset >= link->receive_size) {
                    link->receive_status = ISOTP_RECEIVE_STATUS_FULL;
                } else {
                    /* send fc when bs reaches limit */
                    if (0 == --link->receive_bs_count) {
                        link->receive_bs_count = ISO_TP_DEFAULT_BLOCK_SIZE;
                        isotp_send_flow_control(PCI_FLOW_STATUS_CONTINUE, link->receive_bs_count, ISO_TP_DEFAULT_ST_MIN);
                    }
                }
            }

            break;
        }
        case ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME:
            /* handle fc frame only when sending in progress  */
            if (ISOTP_SEND_STATUS_INPROGRESS != link->send_status) {
                break;
            }

            /* handle message */
            ret = isotp_receive_flow_control_frame(message, frame.payload().length());

            if (ISOTP_RET_OK == ret) {
                /* refresh bs timer */
                link->send_timer_bs = millis->toMSecsSinceEpoch() + ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

                /* overflow */
                if (PCI_FLOW_STATUS_OVERFLOW == message.as.flow_control.FS) {
                    link->send_protocol_result = ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW;
                    link->send_status = ISOTP_SEND_STATUS_ERROR;
                }

                /* wait */
                else if (PCI_FLOW_STATUS_WAIT == message.as.flow_control.FS) {
                    link->send_wtf_count += 1;
                    /* wait exceed allowed count */
                    if (link->send_wtf_count > ISO_TP_MAX_WFT_NUMBER) {
                        link->send_protocol_result = ISOTP_PROTOCOL_RESULT_WFT_OVRN;
                        link->send_status = ISOTP_SEND_STATUS_ERROR;
                    }
                }

                /* permit send */
                else if (PCI_FLOW_STATUS_CONTINUE == message.as.flow_control.FS) {
                    if (0 == message.as.flow_control.BS) {
                        link->send_bs_remain = ISOTP_INVALID_BS;
                    } else {
                        link->send_bs_remain = message.as.flow_control.BS;
                    }
                    link->send_st_min = isotp_st_min_to_ms(message.as.flow_control.STmin);
                    link->send_wtf_count = 0;
                }
            }
            break;
        default:
            break;
    };
}


