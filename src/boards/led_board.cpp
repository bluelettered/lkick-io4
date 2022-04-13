#include "stdinclude.h"

namespace led_board {
    io_packet_t *out;

    io_packet_t *in;
    uint8_t in_size, checksum;

    const component::serial::stream *stream;

    void init(const component::serial::stream *input) {
        stream = input;
        out = io_alloc(PACKAGE_TYPE_RESPONSE, 128);
        in = io_alloc(PACKAGE_TYPE_REQUEST, 128);
    }

    void parse_led_data(const uint8_t* data, int count) {

    }

    void on_packet(io_packet_t *packet) {
        bool response = true;

        io_fill_data(out, packet->srcNodeId, packet->dstNodeId);
        out->response.status = ACK_OK;
        out->response.report = REPORT_OK;
        out->response.command = packet->request.command;

        switch(packet->request.command) {
            case CMD_RESET:
            {
                printf("BOARD RESET\n");
                out->length = 0;
                break;
            }
            case CMD_SET_TIMEOUT:
            {
                auto timeout = packet->request.data[0] << 8 | packet->request.data[1];
                printf("BOARD TIMEOUT: %d\n", timeout);
                out->length = io_build_timeout(out->response.data, 1024, timeout);
                break;
            }
            case CMD_SET_DISABLE:
            {
                printf("BOARD ENABLE STATUS: %d\n", packet->request.data[0]);
                out->length = io_build_set_disable(out->response.data, 1024, packet->request.data[0]);
                break;
            }
            case CMD_SET_LED_DIRECT:
            {
                printf("BOARD RECV LED DATA\n");
                parse_led_data(packet->response.data, packet->length - 3);
                response = false;
                break;
            }
            case CMD_BOARD_INFO:
            {
                printf("REPORTING BOARD INFO\n");
                out->length = io_build_board_info(out->response.data, 1024, "15093-06", "6710A", 0xA0);
                break;
            }
            case CMD_BOARD_STATUS:
            {
                printf("REPORTING BOARD STATUS\n");
                out->length = io_build_board_status(out->response.data, 1024, 0, 0, 0);
                break;
            }
            case CMD_FIRM_SUM:
            {
                printf("REPORTING BOARD FIRMWARE CHECKSUM\n");
                out->length = io_build_firmsum(out->response.data, 1024, 0xAA53);
                break;
            }
            case CMD_PROTOCOL_VERSION:
            {
                printf("REPORTING BOARD PROTOCOL VERSION\n");
                out->length = io_build_protocol_version(out->response.data, 1024, 1, 0);
                break;
            }
            default:
            {
                printf("GOT UNKNOWN PACKAGE\n");
                out->length = 0;
                out->response.status = ACK_INVALID;
                break;
            }
        }

        if (response) {
            io_apply_checksum(out);
            auto size = io_get_package_size(out, PACKAGE_TYPE_RESPONSE);

            stream->write_head();
            for(auto i = 1; i < size; i++) {
                stream->write(out->buffer[i]);
            }

            stream->flush();
        }
    }

    void update() {
        while(stream->available()) {
            uint8_t byte;
            bool is_escaped = stream->read(byte);

            if(byte == 0xE0 && !is_escaped) {
                printf("LED Board: Recv Sync\n");
                in_size = 0;
                checksum = 0;
            }

            in->buffer[in_size++] = byte;

            if(in_size > 5 && in_size - 5 == in->length && checksum == byte) {
                printf("LED Board: Recv %d bytes, checksum %d\n", in_size, checksum);
                on_packet(in);
            }

            if(byte != 0xE0 || is_escaped) {
                checksum += byte;
            }
        }
    }
}