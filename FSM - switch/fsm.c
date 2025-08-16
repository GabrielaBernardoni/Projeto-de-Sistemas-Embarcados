#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/**********************
 * DEFINIÇÕES DO PROTOCOLO
 **********************/
#define STX 0x02
#define ETX 0x03
#define MAX_DATA_SIZE 255

/**********************
 * MÁQUINA DE ESTADOS
 **********************/
typedef enum {
    // Estados do Receptor
    RX_WAIT_STX,
    RX_WAIT_QTD,
    RX_READ_DATA,
    RX_CHECK_CHK,
    RX_WAIT_ETX,
    RX_DONE,
    RX_ERROR,
    
    // Estados do Transmissor
    TX_SEND_STX,
    TX_SEND_QTD,
    TX_SEND_DATA,
    TX_SEND_CHK,
    TX_SEND_ETX,
    TX_DONE,
    TX_ERROR
} ProtocolState;

typedef struct {
    // Receptor
    ProtocolState rx_state;
    uint8_t rx_data[MAX_DATA_SIZE];
    uint8_t rx_expected_bytes;
    uint8_t rx_received_bytes;
    uint8_t rx_calculated_chk;
    
    // Transmissor
    ProtocolState tx_state;
    const uint8_t* tx_data;
    uint8_t tx_data_len;
    uint8_t tx_sent_bytes;
    uint8_t tx_calculated_chk;
} Protocol;

/**********************
 * FUNÇÕES AUXILIARES
 **********************/
uint8_t calculate_checksum(const uint8_t* data, uint8_t length) {
    uint8_t chk = 0;
    for (uint8_t i = 0; i < length; i++) {
        chk ^= data[i];
    }
    return chk;
}

void protocol_init(Protocol* proto) {
    // Inicializa receptor
    proto->rx_state = RX_WAIT_STX;
    proto->rx_received_bytes = 0;
    proto->rx_expected_bytes = 0;
    proto->rx_calculated_chk = 0;
    
    // Inicializa transmissor
    proto->tx_state = TX_SEND_STX;
    proto->tx_sent_bytes = 0;
    proto->tx_calculated_chk = 0;
}

/**********************
 * RECEPTOR (Máquina de Estados)
 **********************/
bool protocol_rx_byte(Protocol* proto, uint8_t byte) {
    switch (proto->rx_state) {
        case RX_WAIT_STX:
            if (byte == STX) {
                proto->rx_state = RX_WAIT_QTD;
                proto->rx_calculated_chk = byte;
            }
            break;
            
        case RX_WAIT_QTD:
            proto->rx_expected_bytes = byte;
            proto->rx_received_bytes = 0;
            proto->rx_state = RX_READ_DATA;
            proto->rx_calculated_chk ^= byte;
            break;
            
        case RX_READ_DATA:
            proto->rx_data[proto->rx_received_bytes++] = byte;
            proto->rx_calculated_chk ^= byte;
            if (proto->rx_received_bytes >= proto->rx_expected_bytes) {
                proto->rx_state = RX_CHECK_CHK;
            }
            break;
            
        case RX_CHECK_CHK:
            if (proto->rx_calculated_chk == byte) {
                proto->rx_state = RX_WAIT_ETX;
            } else {
                proto->rx_state = RX_ERROR;
            }
            break;
            
        case RX_WAIT_ETX:
            if (byte == ETX) {
                proto->rx_state = RX_DONE;
                return true;
            } else {
                proto->rx_state = RX_ERROR;
            }
            break;
            
        case RX_ERROR:
            protocol_init(proto);
            break;
            
        case RX_DONE:
            protocol_init(proto);
            return true;
    }
    
    return false;
}

/**********************
 * TRANSMISSOR (Máquina de Estados)
 **********************/
bool protocol_tx_byte(Protocol* proto, uint8_t* byte) {
    switch (proto->tx_state) {
        case TX_SEND_STX:
            *byte = STX;
            proto->tx_calculated_chk = *byte;
            proto->tx_state = TX_SEND_QTD;
            return false;
            
        case TX_SEND_QTD:
            *byte = proto->tx_data_len;
            proto->tx_calculated_chk ^= *byte;
            proto->tx_state = TX_SEND_DATA;
            proto->tx_sent_bytes = 0;
            return false;
            
        case TX_SEND_DATA:
            *byte = proto->tx_data[proto->tx_sent_bytes++];
            proto->tx_calculated_chk ^= *byte;
            if (proto->tx_sent_bytes >= proto->tx_data_len) {
                proto->tx_state = TX_SEND_CHK;
            }
            return false;
            
        case TX_SEND_CHK:
            *byte = proto->tx_calculated_chk;
            proto->tx_state = TX_SEND_ETX;
            return false;
            
        case TX_SEND_ETX:
            *byte = ETX;
            proto->tx_state = TX_DONE;
            return true;
            
        case TX_ERROR:
            protocol_init(proto);
            return false;
            
        case TX_DONE:
            protocol_init(proto);
            return true;
    }
    
    return false;
}

/**********************
 * TESTES (TDD)
 **********************/
void test_protocol() {
    Protocol proto;
    protocol_init(&proto);
    
    // Teste 1: Pacote válido
    printf("=== Teste 1: Pacote válido ===\n");
    uint8_t valid_packet[] = {STX, 0x02, 0x01, 0x02, 
                             calculate_checksum(valid_packet, 4), ETX};
    
    for (int i = 0; i < sizeof(valid_packet); i++) {
        bool complete = protocol_rx_byte(&proto, valid_packet[i]);
        printf("Byte %d: 0x%02X - Estado: %d - Completo: %d\n", 
              i, valid_packet[i], proto.rx_state, complete);
        if (i == sizeof(valid_packet)-1) {
            assert(complete == true);
        }
    }
    
    // Teste 2: Pacote com erro de checksum
    printf("\n=== Teste 2: Pacote com checksum inválido ===\n");
    protocol_init(&proto);
    uint8_t invalid_packet[] = {STX, 0x02, 0x01, 0x02, 0x00, ETX}; // CHK errado
    
    for (int i = 0; i < sizeof(invalid_packet); i++) {
        bool complete = protocol_rx_byte(&proto, invalid_packet[i]);
        printf("Byte %d: 0x%02X - Estado: %d - Completo: %d\n", 
              i, invalid_packet[i], proto.rx_state, complete);
        if (i == sizeof(invalid_packet)-1) {
            assert(complete == false);
        }
    }
    
    // Teste 3: Transmissão de pacote
    printf("\n=== Teste 3: Transmissão de pacote ===\n");
    protocol_init(&proto);
    uint8_t tx_data[] = {0x01, 0x02};
    proto.tx_data = tx_data;
    proto.tx_data_len = sizeof(tx_data);
    
    uint8_t tx_byte;
    bool tx_complete;
    do {
        tx_complete = protocol_tx_byte(&proto, &tx_byte);
        printf("Byte transmitido: 0x%02X - Completo: %d\n", tx_byte, tx_complete);
    } while (!tx_complete);
}

/**********************
 * FUNCIONAMENTO
 **********************/
int main() {
    // Executa os testes
    test_protocol();
    
    // Exemplo de uso completo
    printf("\n=== Exemplo de uso completo ===\n");
    Protocol proto;
    protocol_init(&proto);
    
    // Dados para transmitir
    uint8_t data_to_send[] = {0x41, 0x42, 0x43}; // "ABC"
    proto.tx_data = data_to_send;
    proto.tx_data_len = sizeof(data_to_send);
    
    // Simulação de transmissão e recepção
    uint8_t tx_buffer[256];
    int tx_index = 0;
    
    // Transmite
    printf("\nTransmitindo...\n");
    uint8_t tx_byte;
    bool tx_done;
    do {
        tx_done = protocol_tx_byte(&proto, &tx_byte);
        tx_buffer[tx_index++] = tx_byte;
        printf("TX: 0x%02X\n", tx_byte);
    } while (!tx_done);
    
    // Recebe
    printf("\nRecebendo...\n");
    protocol_init(&proto);
    bool rx_done;
    for (int i = 0; i < tx_index; i++) {
        rx_done = protocol_rx_byte(&proto, tx_buffer[i]);
        printf("RX: 0x%02X - Estado: %d\n", tx_buffer[i], proto.rx_state);
    }
    
    printf("\nPacote recebido com %s\n", rx_done ? "sucesso" : "erro");
    
    return 0;
}