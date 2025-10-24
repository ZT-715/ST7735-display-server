#define _DEFAULT_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

// sudo apt install libcurl4-openssl-dev
// add -lcurl to linker flags
#include <curl/curl.h>

// sudo apt install libcjson1 libcjson-dev
// add -lcjson to linker flag
#include <cjson/cJSON.h> // Inclui o header da cJSON

// Compile
// gcc main_influx.c -o server_influx.o -lcurl -lcjson -std=c99

// #define MAX_DIGITS 10
// #define PKT_BUFFER_SIZE 256
// #define PORT 9123
// #define MAX_CLIENTS 20


typedef enum {
    ID_UNKNOWN = 0, // Um valor padrão para sensores não reconhecidos
    ID_AHT10   = 1,
    ID_BMP280  = 2
} sensor_id_t;

typedef struct received_data_parsed {
    int id;
    float temperature;
    float humidity;
    float pressure;
    time_t timestamp;
} packet_t;


/**
 * @brief Converte o nome de um sensor (string) para seu ID numérico (enum).
 *
 * @param name A string com o nome do sensor (ex: "AHT10").
 * @return O valor do enum correspondente (ex: ID_AHT10) ou ID_UNKNOWN.
 */
sensor_id_t get_sensor_id_from_name(const char* name) {
    if (name == NULL) {
        return ID_UNKNOWN;
    }
    if (strcmp(name, "AHT10") == 0) {
        return ID_AHT10;
    }
    if (strcmp(name, "BMP280") == 0) {
        return ID_BMP280;
    }
    return ID_UNKNOWN; // Retorna 0 se o nome não for reconhecido
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
    switch (prev->id) {
        case ID_AHT10:
            printf("ID AHT10 |");
            break;
        case ID_BMP280:
            printf("ID BMP280|");
            break;
        default:
            printf("ID       |");
            break;
    };
    switch (last->id) {
        case ID_AHT10:
            printf(" AHT10   \n");
            break;
        case ID_BMP280:
            printf(" BMP280  \n");
            break;
        default:
            printf("         \n");
            break;
    }
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
    if (prev->pressure != 0.0f) printf("P %4.0f |", prev->pressure); else printf("P        |");
    if (last->pressure != 0.0f) printf(" %3.0f hPa\n", last->pressure); else printf("      hPa\n");
    printf("         |          \n");

    printf("–––––––––|––––––––––\n\nÚltimo update:\n\n");

    // Timestamps
    char time_str[9]; // For HH:MM:SS\0
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&prev->timestamp));
    switch (prev->id) {
        case ID_AHT10:
            printf(" AHT10  %s\n", time_str);
            break;
        case ID_BMP280:
            printf(" BPM280 %s\n", time_str);
            break;
        default:
            printf(" ID %2i:  %s\n", prev->id, time_str);
            break;
    };
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&last->timestamp));
    switch (last->id) {
        case ID_AHT10:
            printf(" AHT10  %s\n", time_str);
            break;
        case ID_BMP280:
            printf(" BPM280 %s\n", time_str);
            break;
        default:
            printf(" ID %2i:  %s\n", prev->id, time_str);
            break;

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

// Estrutura auxiliar para guardar a resposta da libcurl em memória
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Função de callback que a libcurl usa para salvar os dados recebidos na memória.
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int main(void) {
    CURL *curl = curl_easy_init();
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1); // Será expandido conforme necessário
    chunk.size = 0;

    if(!curl) {
        perror("curl_easy_init");
        exit(EXIT_FAILURE);
    }

    char *query = strdup("");
    if (!query) {
        perror("strdup");
        return EXIT_FAILURE;
    }

    data_append(&query, "db", "embarcados2025");
    data_append(&query, "q", "SELECT last(tempoamostra) as timestamp, last(pressao) as pressure, last(temperatura) as temperature, last(umidade) as humidity FROM sensor GROUP BY ID");

    // Configura a requisição libcurl
    curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.11:8086/query?pretty=false&u=embarcados&p=embarcados");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    // Executa a requisição
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform(): %s\n", curl_easy_strerror(res));
    }

    free(query);
    curl_easy_cleanup(curl);

    cJSON *root = cJSON_Parse(chunk.memory);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Erro no parsing do JSON: %s\n", error_ptr);
        }
        free(chunk.memory);
        return 1;
    }

    cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");
    cJSON *first_result = cJSON_GetArrayItem(results, 0);
    cJSON *series = cJSON_GetObjectItemCaseSensitive(first_result, "series");

    int series_count = cJSON_GetArraySize(series);
    if (series_count == 0) {
        printf("Nenhuma série de dados retornada.\n");
        cJSON_Delete(root);
        free(chunk.memory);
        return 0;
    }

    packet_t **packet_list = malloc(sizeof(packet_t*) * series_count);

    cJSON *s;
    int i = 0;
    cJSON_ArrayForEach(s, series) {
        packet_list[i] = malloc(sizeof(packet_t));
        packet_t *current_packet = packet_list[i];
        memset(current_packet, 0, sizeof(packet_t));

        // Atribui o ID para o pacote recebido
        cJSON* tags = cJSON_GetObjectItemCaseSensitive(s, "tags");
        cJSON* id_tag_json = cJSON_GetObjectItemCaseSensitive(tags, "ID");
        if (cJSON_IsString(id_tag_json) && (id_tag_json->valuestring != NULL)) {
            char *sensor_name = id_tag_json->valuestring;
            current_packet->id = get_sensor_id_from_name(sensor_name);
        } else {
            current_packet->id = ID_UNKNOWN;
        }

        cJSON *columns = cJSON_GetObjectItemCaseSensitive(s, "columns");
        cJSON *values = cJSON_GetObjectItemCaseSensitive(s, "values");
        cJSON *first_value_set = cJSON_GetArrayItem(values, 0);

        int col_idx = 0;
        cJSON *col_name_json;
        cJSON_ArrayForEach(col_name_json, columns) {
            char *col_name = cJSON_GetStringValue(col_name_json);
            cJSON *value = cJSON_GetArrayItem(first_value_set, col_idx);

            if (strcmp(col_name, "timestamp") == 0 && !cJSON_IsNull(value)) {
                current_packet->timestamp = (time_t)cJSON_GetNumberValue(value);
            } else if (strcmp(col_name, "pressure") == 0 && !cJSON_IsNull(value)) {
                current_packet->pressure = (float)cJSON_GetNumberValue(value);
            } else if (strcmp(col_name, "temperature") == 0 && !cJSON_IsNull(value)) {
                current_packet->temperature = (float)cJSON_GetNumberValue(value);
            } else if (strcmp(col_name, "humidity") == 0 && !cJSON_IsNull(value)) {
                current_packet->humidity = (float)cJSON_GetNumberValue(value);
            }
            col_idx++;
        }
        i++;
    }

    packet_t packets_to_print[2] = {0};
    if (series_count > 0) {
        packets_to_print[0] = *packet_list[0];
    }
    if (series_count > 1) {
        packets_to_print[1] = *packet_list[1];
    }
    print_table(packets_to_print);

    cJSON_Delete(root);

    for (int j = 0; j < series_count; j++) {
        free(packet_list[j]);
    }
    free(packet_list);
    free(chunk.memory);

    return 0;
}