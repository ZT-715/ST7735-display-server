#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>

#define MAX_DIGITS 10
#define PKT_BUFFER_SIZE 256
#define PORT 9123
#define MAX_CLIENTS 20

typedef struct received_data_parsed {
    int id;
    float temperature;
    float humidity;
    float pressure;
    time_t timestamp;
} packet_t;

/**
 * @brief Parses a string packet into a packet_t structure.
 *
 * @param packet The raw character buffer received from the socket.
 * @param length The length of the data in the buffer.
 * @return A packet_t struct with the parsed values.
 */
packet_t parse_packet_string(const char* const packet, const int length) {
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
	    case('-'):
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
    result.id = atoi(id);
    result.temperature = atof(temperature);
    result.humidity = atof(humidity);
    result.pressure = atof(pressure);
    result.timestamp = time(NULL);
    return result;
}

/**
 * @brief Clears the console and prints the formatted data table for a 20x16 display.
 * @param packets An array of size 2, where packets[0] is the last received packet
 * and packets[1] is the previous one from a different ID.
 */
void print_table(packet_t packets[2]) {
    packet_t* last;
    packet_t* prev;
    if (packets[0].id > packets[1].id) {
        last = &packets[0];
        prev = &packets[1];
    }
    else {
         last = &packets[1];
         prev = &packets[0];
    }

    // ANSI escape codes to clear screen and move cursor to top-left
    printf("\033[H\033[J");

    printf("–––––––––|––––––––––\n");
    // ID row
    if (prev->id > 0) printf("ID   %3i |", prev->id); else printf("ID       |");
    if (last->id > 0) printf("     %4i\n", last->id); else printf("         \n");
    printf("–––––––––|––––––––––\n");

    // Temperature row
    if (prev->temperature != 0.0f) printf("T %6.2f |", prev->temperature); else printf("T        |");
    if (last->temperature != 0.0f) printf(" %6.2f C\n", last->temperature); else printf("        C\n");
    printf("         |          \n");

    // Humidity row
    if (prev->humidity != 0.0f) printf("H %6.2f |", prev->humidity); else printf("H        |");
    if (last->humidity != 0.0f) printf(" %6.2f %%\n", last->humidity); else printf("        %%\n");
    printf("         |          \n");

    // Pressure row
    if (prev->pressure != 0.0f) printf("P %4.1f  |", prev->pressure); else printf("P        |");
    if (last->pressure != 0.0f) printf(" %4.1f kPa\n", last->pressure); else printf("      kPa\n");
    printf("         |          \n");

    printf("–––––––––|––––––––––\n\nÚltimo update:\n\n");

    // Timestamps
    char time_str[9]; // For HH:MM:SS\0
    if (last->id > 0) {
        strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&last->timestamp));
        printf("ID %2i:   %s\n", last->id, time_str);
    } else {
        printf("ID --:   Aguardando\n");
    }

    if (prev->id > 0) {
        strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&prev->timestamp));
        printf("ID %2i:   %s\n", prev->id, time_str);
    } else {
        printf("ID --:   Aguardando\n");
    }
    fflush(stdout); // Ensure output is immediately written to the display
}


int main(void) {
    // Cria uma variável para o file descriptor do socket
    static int server_fd, new_socket;
    // estrutura para endereço inter-usuário (internet)
    struct sockaddr_in address;

    // Atribui a 'server_fd' um descritor de arquivo referente a um socket
    // para conexão no domínio de endereços da família do protocolo de
    // InterNET v4 (IPv4) com sockets do tipo 'stream', que criam uma
    // conexão com outro socket, assim como utilizado no TCP. Com o parametro
    // protocolo definido como 0, o sistema supõe automaticamente TCP com
    // base no tipo e domínio.
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        // Se socket() retornar erro (menor que 0), perror
        // imprime em stderr a mensagem passada, seguido de
        // uma descrição do erro definido em 'errno' por socket().
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Option '1' em setsockopt() define configurações passadas como ativas
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    // setsockopt() recebe um file descriptor, o nível da configuração
    // (SOL = SOcket Level), e as configurações a serem alteradas para
    // o estado de opt, no caso, REUSEADDR e REUSEPORT.
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // Define estrutura de endereço para o socket
    //     Address Family é InterNET protocol v4 (IPV4)
    //     Socket Address é qualquer interface disponível (localhost e outros)
    //     Porta para é 'PORT', onde hton converte de um endereço no formato
    // utilizado pelo host para o padrão de rede (network)
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Realiza o bind do socket de 'server_fd' para o endereço definido em 'address'
    if (bind(server_fd, (struct sockaddr*)&address,
             sizeof(address))
        < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Abre (começa 'ouvir') porta definida em 'server_fd' e cria backlog
    // para conexões de tamanho 3
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
  // Array to hold the last two unique packets for display. Initialize to zero.
    packet_t last_packets[2] = {{0}, {0}};

    // Array of pollfd structures. Size is max clients + 1 for the listening socket.
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1; // Current number of file descriptors in the array

    // Initialize the polling array with the listening socket
    fds[0].fd = server_fd;
    fds[0].events = POLLIN; // Monitor for incoming connection requests

    print_table(last_packets); // Initial print of the empty table

    // --- MAIN SERVER LOOP ---
    while (1) {
        // poll() waits for an event on one of the sockets. Timeout is -1 (infinite).
        int ret = poll(fds, nfds, -1);
        if (ret < 0) {
            perror("poll error");
            break;
        }

        // New connections ---
        if (fds[0].revents & POLLIN) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
                perror("accept");
            } else {
                if (nfds == MAX_CLIENTS) {
                    for (int i = MAX_CLIENTS; i > MAX_CLIENTS/2; i--) {
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        fds[i].events = 0;
                        nfds--;
                    }
                }
                fds[nfds].fd = new_socket;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        // Handle data
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                char packet_buffer[PKT_BUFFER_SIZE] = {0};
                ssize_t status_read = read(fds[i].fd, packet_buffer, PKT_BUFFER_SIZE - 1);

                if (status_read > 0) {
                    packet_t new_packet = parse_packet_string(packet_buffer, status_read);
                    new_packet.timestamp = time(NULL); // Add timestamp on arrival

                    if (new_packet.id > 0) {
                        // SWAP LOGIC: If the new packet's ID is different from the last one,
                        // move the current last one to the 'previous' slot (left column).
                        if (new_packet.id != last_packets[0].id) {
                            last_packets[1] = last_packets[0];
                        }
                        // The new packet always becomes the 'last' one (right column).
                        last_packets[0] = new_packet;
                        print_table(last_packets);
                    }
                    send(fds[i].fd, "OK\n", 3, 0); // Acknowledge receipt

                } else {
                    // If read returns 0, the client has closed the connection.
                    // If read returns < 0, an error occurred.
                    close(fds[i].fd);
                    // Remove the client from the poll array by replacing it with the last one.
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--; // Decrement 'i' to re-check the fd that was just moved.
                }
            }
        }
    }

    // Close all sockets
    for (int i = 0; i < nfds; i++) {
        close(fds[i].fd);
    }

    return 0;
}