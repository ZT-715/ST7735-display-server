#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

#define MAX_DIGITS 10

typedef struct received_data_parsed {
    int id;
    float temperature;
    float humidity;
    float pressure;
} packet_t;

packet_t parse_packet_string(char* packet, int length) {
    char ch = ' ';
    char lead = ' ';
    char id[MAX_DIGITS] = {'\0'},
        temperature[MAX_DIGITS] = {'\0'},
        humidity[MAX_DIGITS] = {'\0'},
        pressure[MAX_DIGITS] = {'\0'};
    int idx_id = 0, idx_temp = 0, idx_hum = 0, idx_press = 0;
    for (int i = 0; i < length; i++) {
        ch = packet[i];
        switch (ch) {
            case('I'):
            case('T'):
            case('H'):
            case('P'):
                lead = ch;
                break;
            case('0'):
            case('1'):
            case('2'):
            case('3'):
            case('4'):
            case('5'):
            case('6'):
            case('7'):
            case('8'):
            case('9'):
            case('.'):
                if (lead == 'I' && idx_id < MAX_DIGITS - 2) {
                    id[idx_id] = ch;
                    idx_id++;
                }
                else if (lead == 'T' && idx_temp < MAX_DIGITS - 2) {
                    temperature[idx_temp] = ch;
                    idx_temp++;
                }
                else if (lead == 'H' && idx_hum < MAX_DIGITS - 2) {
                    humidity[idx_hum] = ch;
                    idx_hum++;
                }
                else if (lead == 'P' && idx_press < MAX_DIGITS - 2) {
                    pressure[idx_press] = ch;
                    idx_press++;
                }
            default:
                break;
        }
    }
    // convert stod()
    packet_t result = {0};
    result.id = atof(id);
    result.temperature = atof(temperature);
    result.humidity = atof(humidity);
    result.pressure = atof(pressure);
    return result;
}

int main(void) {
    char packet_buffer[256] = {"ID01\n \
                                T100.15\n \
                                H0.11\n \
                                P1000"};
    packet_t result = parse_packet_string(packet_buffer, 256);
    for (int i = 0; i < 16; i++) {
        printf("\n");
    }
    printf("–––––––––|––––––––––\n");
    printf("ID %5i | %8i\n", result.id, result.id + 1);
    printf("–––––––––|––––––––––\n");
    printf("T %6.2f | %5.2f ºC\n", result.temperature, result.temperature);
    printf("         |          \n");
    printf("H %6.2f | %6.2f  \%\n", result.humidity, result.humidity);
    printf("         |          \n");
    printf("P %6.0f | %5.0f mpa\n", result.pressure, result.pressure);
    printf("         |          \n");
    printf("–––––––––|––––––––––\n\nÚltimo update:\n");
    printf("\nID 1:   10 min atrás\nID 2:    2 min atrás\n");
    return 0;
}