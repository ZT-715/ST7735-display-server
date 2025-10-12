#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include <curl/curl.h>

#define MAX_DIGITS 10
// #define PKT_BUFFER_SIZE 256
// #define PORT 9123
// #define MAX_CLIENTS 20

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

/**
 * Appends a URL-encoded key-value pair to a string.
 * This function correctly handles reallocating memory and adding '&' only when needed.
 */
static int data_append(char** string_source, const char *name, const char *field) {
    char *escaped_field = curl_easy_escape(NULL, field, 0);
    if (!escaped_field) {
        return 0;
    }

    size_t source_len = strlen(*string_source);

    // Determine the separator: "" for the first pair, "&" for subsequent pairs
    const char* separator = (source_len > 0) ? "&" : "";

    // Calculate the length of the new part: "separator" + "name" + "=" + "escaped_field"
    size_t part_len = strlen(separator) + strlen(name) + 1 + strlen(escaped_field);

    // Reallocate memory
    char *new_string = realloc(*string_source, source_len + part_len + 1);
    if (!new_string) {
        fprintf(stderr, "realloc() failed\n");
        curl_free(escaped_field);
        return 0;
    }

    // Append the new part
    sprintf(new_string + source_len, "%s%s=%s", separator, name, escaped_field);

    // Update the original pointer
    *string_source = new_string;

    curl_free(escaped_field);
    return 1;
}

int main(void) {
    CURL *curl = curl_easy_init();
    CURLcode res;

    if(!curl) {
        perror("curl_easy_init");
        exit(EXIT_FAILURE);
    }

    // strdup to create heap-allocated string is safer than malloc
    char *query = strdup("");
    if (!query) {
        perror("strdup");
        return EXIT_FAILURE;
    }

    // Data fields
    data_append(&query, "db", "embarcados2025");
    data_append(&query, "q", "SELECT * FROM sensor");

    printf("http://192.168.1.11:8086/query\?pretty=true&u=embarcados&p=embarcados\n");
    printf("%s\n", query);

    // Setup URL
    curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.11:8086/query\?pretty=true&u=embarcados&p=embarcados");
    // curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "db=embarcados2025&q=SELECT+%2A+FROM+sensor");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);

    // Request
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform(): %s\n", curl_easy_strerror(res));
    }

    free(query);
    curl_easy_cleanup(curl);
}