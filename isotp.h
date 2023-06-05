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

#ifndef ISOTP_H
#define ISOTP_H

#include <QObject>
#include <QCanBusFrame>
#include <QDebug>
#include <QTimer>

/* Max number of messages the receiver can receive at one time, this value
 * is affectied by can driver queue length
 */
#define ISO_TP_DEFAULT_BLOCK_SIZE   8

/* The STmin parameter value specifies the minimum time gap allowed between
 * the transmission of consecutive frame network protocol data units
 */
#define ISO_TP_DEFAULT_ST_MIN       0

/* This parameter indicate how many FC N_PDU WTs can be transmitted by the
 * receiver in a row.
 */
#define ISO_TP_MAX_WFT_NUMBER       1

/* Private: The default timeout to use when waiting for a response during a
 * multi-frame send or receive.
 */
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT 100

/* Private: Determines if by default, padding is added to ISO-TP message frames.
 */
#define ISO_TP_FRAME_PADDING

class Isotp : public QObject
{
    Q_OBJECT
public:
    explicit Isotp(QObject *parent = nullptr);
    ~Isotp();

    void init_link(quint32 sendId, quint16 sendBufSize, quint16 recvBufSize, quint16 pollInterval);
    void send(QByteArray payload);
    void send_with_id(quint32 id, QByteArray payload);


public slots:
    void on_can_message(QCanBusFrame frame);


private:
    typedef struct IsoTpLink {
        /* sender paramters */
        quint32                    send_arbitration_id; /* used to reply consecutive frame */
        /* message buffer */
        quint8*                    send_buffer;
        quint16                    send_buf_size;
        quint16                    send_size;
        quint16                    send_offset;
        /* multi-frame flags */
        quint8                     send_sn;
        quint16                    send_bs_remain; /* Remaining block size */
        quint8                     send_st_min;    /* Separation Time between consecutive frames, unit millis */
        quint8                     send_wtf_count; /* Maximum number of FC.Wait frame transmissions  */
        quint32                    send_timer_st;  /* Last time send consecutive frame */
        quint32                    send_timer_bs;  /* Time until reception of the next FlowControl N_PDU
                                                       start at sending FF, CF, receive FC
                                                       end at receive FC */
        int                         send_protocol_result;
        quint8                     send_status;

        /* receiver paramters */
        quint32                    receive_arbitration_id;
        /* message buffer */
        quint8*                    receive_buffer;
        quint16                    receive_buf_size;
        quint16                    receive_size;
        quint16                    receive_offset;
        /* multi-frame control */
        quint8                     receive_sn;
        quint8                     receive_bs_count; /* Maximum number of FC.Wait frame transmissions  */
        quint32                    receive_timer_cr; /* Time until transmission of the next ConsecutiveFrame N_PDU
                                                         start at sending FC, receive CF
                                                         end at receive FC */
        int                         receive_protocol_result;
        quint8                     receive_status;
    } IsoTpLink;

    IsoTpLink* link = nullptr;

    /**************************************************************
     * compiler specific defines
     *************************************************************/
    #ifdef __GNUC__
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define ISOTP_BYTE_ORDER_LITTLE_ENDIAN
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #else
    #error "unsupported byte ordering"
    #endif
    #endif

    /**************************************************************
     * OS specific defines
     *************************************************************/
    #ifdef _WIN32
    #define snprintf _snprintf
    #endif

    #ifdef _WIN32
    #define ISOTP_BYTE_ORDER_LITTLE_ENDIAN
    #define __builtin_bswap8  _byteswap_uint8
    #define __builtin_bswap16 _byteswap_uint16
    #define __builtin_bswap32 _byteswap_uint32
    #define __builtin_bswap64 _byteswap_uint64
    #endif

    /**************************************************************
     * internal used defines
     *************************************************************/
    #define ISOTP_RET_OK           0
    #define ISOTP_RET_ERROR        -1
    #define ISOTP_RET_INPROGRESS   -2
    #define ISOTP_RET_OVERFLOW     -3
    #define ISOTP_RET_WRONG_SN     -4
    #define ISOTP_RET_NO_DATA      -5
    #define ISOTP_RET_TIMEOUT      -6
    #define ISOTP_RET_LENGTH       -7

    /* return logic true if 'a' is after 'b' */
    #define IsoTpTimeAfter(a,b) ((int32_t)((int32_t)(b) - (int32_t)(a)) < 0)

    /*  invalid bs */
    #define ISOTP_INVALID_BS       0xFFFF

    /* ISOTP sender status */
    typedef enum {
        ISOTP_SEND_STATUS_IDLE,
        ISOTP_SEND_STATUS_INPROGRESS,
        ISOTP_SEND_STATUS_ERROR,
    } IsoTpSendStatusTypes;

    /* ISOTP receiver status */
    typedef enum {
        ISOTP_RECEIVE_STATUS_IDLE,
        ISOTP_RECEIVE_STATUS_INPROGRESS,
        ISOTP_RECEIVE_STATUS_FULL,
    } IsoTpReceiveStatusTypes;

    /* can fram defination */
    #if defined(ISOTP_BYTE_ORDER_LITTLE_ENDIAN)
    typedef struct {
        uint8_t reserve_1:4;
        uint8_t type:4;
        uint8_t reserve_2[7];
    } IsoTpPciType;

    typedef struct {
        uint8_t SF_DL:4;
        uint8_t type:4;
        uint8_t data[7];
    } IsoTpSingleFrame;

    typedef struct {
        uint8_t FF_DL_high:4;
        uint8_t type:4;
        uint8_t FF_DL_low;
        uint8_t data[6];
    } IsoTpFirstFrame;

    typedef struct {
        uint8_t SN:4;
        uint8_t type:4;
        uint8_t data[7];
    } IsoTpConsecutiveFrame;

    typedef struct {
        uint8_t FS:4;
        uint8_t type:4;
        uint8_t BS;
        uint8_t STmin;
        uint8_t reserve[5];
    } IsoTpFlowControl;

    #else

    typedef struct {
        uint8_t type:4;
        uint8_t reserve_1:4;
        uint8_t reserve_2[7];
    } IsoTpPciType;

    /*
    * single frame
    * +-------------------------+-----+
    * | byte #0                 | ... |
    * +-------------------------+-----+
    * | nibble #0   | nibble #1 | ... |
    * +-------------+-----------+ ... +
    * | PCIType = 0 | SF_DL     | ... |
    * +-------------+-----------+-----+
    */
    typedef struct {
        uint8_t type:4;
        uint8_t SF_DL:4;
        uint8_t data[7];
    } IsoTpSingleFrame;

    /*
    * first frame
    * +-------------------------+-----------------------+-----+
    * | byte #0                 | byte #1               | ... |
    * +-------------------------+-----------+-----------+-----+
    * | nibble #0   | nibble #1 | nibble #2 | nibble #3 | ... |
    * +-------------+-----------+-----------+-----------+-----+
    * | PCIType = 1 | FF_DL                             | ... |
    * +-------------+-----------+-----------------------+-----+
    */
    typedef struct {
        uint8_t type:4;
        uint8_t FF_DL_high:4;
        uint8_t FF_DL_low;
        uint8_t data[6];
    } IsoTpFirstFrame;

    /*
    * consecutive frame
    * +-------------------------+-----+
    * | byte #0                 | ... |
    * +-------------------------+-----+
    * | nibble #0   | nibble #1 | ... |
    * +-------------+-----------+ ... +
    * | PCIType = 0 | SN        | ... |
    * +-------------+-----------+-----+
    */
    typedef struct {
        uint8_t type:4;
        uint8_t SN:4;
        uint8_t data[7];
    } IsoTpConsecutiveFrame;

    /*
    * flow control frame
    * +-------------------------+-----------------------+-----------------------+-----+
    * | byte #0                 | byte #1               | byte #2               | ... |
    * +-------------------------+-----------+-----------+-----------+-----------+-----+
    * | nibble #0   | nibble #1 | nibble #2 | nibble #3 | nibble #4 | nibble #5 | ... |
    * +-------------+-----------+-----------+-----------+-----------+-----------+-----+
    * | PCIType = 1 | FS        | BS                    | STmin                 | ... |
    * +-------------+-----------+-----------------------+-----------------------+-----+
    */
    typedef struct {
        uint8_t type:4;
        uint8_t FS:4;
        uint8_t BS;
        uint8_t STmin;
        uint8_t reserve[5];
    } IsoTpFlowControl;

    #endif

    typedef struct {
        uint8_t ptr[8];
    } IsoTpDataArray;

    typedef struct {
        union {
            IsoTpPciType          common;
            IsoTpSingleFrame      single_frame;
            IsoTpFirstFrame       first_frame;
            IsoTpConsecutiveFrame consecutive_frame;
            IsoTpFlowControl      flow_control;
            IsoTpDataArray        data_array;
        } as;
    } IsoTpCanMessage;

    /**************************************************************
     * protocol specific defines
     *************************************************************/

    /* Private: Protocol Control Information (PCI) types, for identifying each frame of an ISO-TP message.
     */
    typedef enum {
        ISOTP_PCI_TYPE_SINGLE             = 0x0,
        ISOTP_PCI_TYPE_FIRST_FRAME        = 0x1,
        TSOTP_PCI_TYPE_CONSECUTIVE_FRAME  = 0x2,
        ISOTP_PCI_TYPE_FLOW_CONTROL_FRAME = 0x3
    } IsoTpProtocolControlInformation;

    /* Private: Protocol Control Information (PCI) flow control identifiers.
     */
    typedef enum {
        PCI_FLOW_STATUS_CONTINUE = 0x0,
        PCI_FLOW_STATUS_WAIT     = 0x1,
        PCI_FLOW_STATUS_OVERFLOW = 0x2
    } IsoTpFlowStatus;

    /* Private: network layer resault code.
     */
    #define ISOTP_PROTOCOL_RESULT_OK            0
    #define ISOTP_PROTOCOL_RESULT_TIMEOUT_A    -1
    #define ISOTP_PROTOCOL_RESULT_TIMEOUT_BS   -2
    #define ISOTP_PROTOCOL_RESULT_TIMEOUT_CR   -3
    #define ISOTP_PROTOCOL_RESULT_WRONG_SN     -4
    #define ISOTP_PROTOCOL_RESULT_INVALID_FS   -5
    #define ISOTP_PROTOCOL_RESULT_UNEXP_PDU    -6
    #define ISOTP_PROTOCOL_RESULT_WFT_OVRN     -7
    #define ISOTP_PROTOCOL_RESULT_BUFFER_OVFLW -8
    #define ISOTP_PROTOCOL_RESULT_ERROR        -9


    quint8 isotp_ms_to_st_min(quint8 ms);
    quint8 isotp_st_min_to_ms(quint8 st_min);
    int isotp_send_flow_control(quint8 flow_status, quint8 block_size, quint8 st_min_ms);
    int isotp_send_single_frame(quint32 id);
    int isotp_send_first_frame(quint32 id);
    int isotp_send_consecutive_frame();
    int isotp_receive_single_frame(IsoTpCanMessage &message, quint8 len);
    int isotp_receive_first_frame(IsoTpCanMessage &message, quint8 len);
    int isotp_receive_consecutive_frame(IsoTpCanMessage &message, quint8 len);
    int isotp_receive_flow_control_frame(IsoTpCanMessage &message, quint8 len);
    void poll();
    int receive(QByteArray &payload);
    void on_timeout();

    QTimer *pollTimer = nullptr;

    quint8 *sendbuf = nullptr;
    quint8 *recvbuf = nullptr;

signals:

    void send_can(QCanBusFrame frame);
    void new_message(QByteArray message);
};

#endif // ISOTP_H
