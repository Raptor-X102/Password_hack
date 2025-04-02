#include "Patcher.h"

const int Tmp_buffer_size = 8;

int Keygen_inject() {

    /*char* read_files[1] = {};
    file_types files = {};
    files.read_files = read_files;
    if(!Check_r_w_flags(CHECK_R, argv, argc, &files)) {

        DEBUG_PRINTF("ERROR: flags verification failed\n")
        return 0;
    }*/
    char file_to_hack[] = "PASSWO_1.COM";
    int64_t file_size = get_file_size(file_to_hack);
    FILE* input_file = fopen(file_to_hack, "rb");
    char* buffer = (char*) calloc(file_size+1, sizeof(char));
    if(!buffer) {

        DEBUG_PRINTF("ERROR: memory was not allocated\n");
        return 0;
    }
    fread(buffer, sizeof(char), file_size, input_file);
    buffer[file_size] = '\0';
    fclose(input_file);
    const char str_to_find[] = {0x74, 0x0F, -112, -112, -24, -49, '\0'};
    char tmp_buffer[Tmp_buffer_size] = {};
    int i = 0;
    for(; i < file_size - (Tmp_buffer_size); i++) {

        memcpy(tmp_buffer, buffer + i, Tmp_buffer_size - 1);
        if(!strcmp(tmp_buffer, str_to_find)) {

            buffer[i] = 0x75;
            break;
        }
    }
    if(i == file_size - (Tmp_buffer_size)) {

        DEBUG_PRINTF("ERROR: string was not found\n");
        return 0;
    }

    input_file = fopen(file_to_hack, "wb");
    fwrite(buffer, sizeof(char), file_size, input_file);
    fclose(input_file);
    return 1;
}


