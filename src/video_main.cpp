#define SDL_MAIN_USE_CALLBACKS
#include <string.h>
#include <stdio.h>
#include <math.h>
extern "C" {
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/pixfmt.h>
#include <libavutil/channel_layout.h>
}
#include "Debug_printf.h"
#include "Patcher.h"

struct Button {
    SDL_Rect rect;
    const char* label;
    SDL_Texture* textTexture;
    int text_width, text_height;
    bool is_clicked;
};

struct AppState {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    Button button;

    AVFormatContext* formatContext;
    AVFormatContext* audioFormatContext;
    AVCodecParameters* videoCodecParams;
    AVCodecParameters* audioCodecParams;
    const AVCodec* videoCodec;
    const AVCodec* audioCodec;
    int videoStreamIndex;
    int audioStreamIndex;
    AVCodecContext* videoCodecContext;
    AVCodecContext* audioCodecContext;
    SDL_Texture* currentFrameTexture;
    SDL_Texture* previousFrameTexture;
    SwsContext* swsCtx;
    AVFrame* rgbFrame;
    AVPacket* packet;
    AVFrame* frame;
    AVFrame* yuvFrame;

    SDL_AudioDeviceID audioDevice;
    SDL_AudioSpec audioSpec;
    Mix_Chunk* audio_chunk;
    int audio_channel;
    uint8_t audio_buffer[192000];

    double last_tick;
    double current_tick;
    double delta_time;
};


const int Width = 800, Height = 600;
int Keygen_inject();
void drawButton(SDL_Renderer* renderer, Button* button);
bool isButtonClicked(int x, int y, Button* button);
void UpdateButtonPosition(Button* button);
void RenderButton(SDL_Renderer* renderer, Button* button, SDL_Rect* buttonRect);
void Render_bckgrnd(SDL_Renderer* renderer, AppState* appState);
void convert_float_to_s16(const float* src, int16_t* dst, int num_samples);
SDL_Texture* CreateTextTexture(SDL_Renderer* renderer, Button* button, TTF_Font* font, SDL_Color color, int maxWidth, int maxHeight);
//int audio_decode_frame(AppState* appState, AVCodecContext* codecCtx, uint8_t* audio_buf, int buf_size);
int audio_decode_and_play(AppState* appState);
void cleanup(AppState* appState);
bool initializeVideoAndAudio(AppState* appState);
void handleButtonClick(AppState* appState);
void changeButtonText(SDL_Renderer* renderer, Button* button, TTF_Font* font, const char* newText);
void sync_audio_and_video(AppState* appState);

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(__attribute__((unused)) HINSTANCE hInstance,
                   __attribute__((unused)) HINSTANCE hPrevInstance,
                   __attribute__((unused)) LPSTR lpCmdLine,
                   __attribute__((unused)) int nCmdShow) {
    return SDL_main(__argc, __argv);
}
#endif

/*void Render_bckgrnd(SDL_Renderer* renderer, AppState* appState) {
    static bool frameReady = false;
    static int64_t lastPts = 0;
    static Uint32 lastFrameTime = 0;
    static double timeBase = av_q2d(appState->formatContext->streams[appState->videoStreamIndex]->time_base);

    if (!frameReady) {
        // Буферизация пакетов и отправка в декодер
        while (av_read_frame(appState->formatContext, appState->packet) >= 0) {
            if (appState->packet->stream_index == appState->videoStreamIndex) {
                int ret = avcodec_send_packet(appState->videoCodecContext, appState->packet);
                if (ret < 0) {
                    SDL_Log("Error sending video packet: %s", av_err2str(ret));
                    av_packet_unref(appState->packet);
                    continue;
                }
            }
            av_packet_unref(appState->packet); // Освобождаем пакет после отправки
        }

        // Получаем кадры из декодера
        while (true) {
            int ret = avcodec_receive_frame(appState->videoCodecContext, appState->frame);
            if (ret == AVERROR(EAGAIN)) {
                SDL_Log("No frame available (EAGAIN)");
                break;
            } else if (ret == AVERROR_EOF) {
                SDL_Log("End of video stream (EOF)");
                av_seek_frame(appState->formatContext, appState->videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(appState->videoCodecContext);
                break;
            } else if (ret < 0) {
                SDL_Log("Error receiving video frame: %s", av_err2str(ret));
                break;
            }

            // Получаем временную метку (PTS) кадра
            int64_t pts = appState->frame->pts;
            if (pts == AV_NOPTS_VALUE) {
                pts = 0; // Если PTS отсутствует, используем 0
            }

            // Вычисляем задержку между кадрами
            if (lastPts != 0) {
                double frameDelay = (pts - lastPts) * timeBase * 1000; // Время между кадрами в миллисекундах
                Uint32 currentTime = SDL_GetTicks();
                Uint32 elapsedTime = currentTime - lastFrameTime;

                // Логируем PTS и задержку
                SDL_Log("Frame PTS: %ld, Delay: %f ms", pts, frameDelay);

                // Если кадр отображается слишком быстро, добавляем задержку
                if (elapsedTime < frameDelay) {
                    SDL_Delay((Uint32)(frameDelay - elapsedTime));
                }
            }

            // Обновляем временные метки
            lastPts = pts;
            lastFrameTime = SDL_GetTicks();

            // Преобразование кадра в RGB
            sws_scale(appState->swsCtx, (uint8_t const* const*)appState->frame->data, appState->frame->linesize,
                      0, appState->videoCodecContext->height,
                      appState->rgbFrame->data, appState->rgbFrame->linesize);

            // Обновление текстуры SDL
            SDL_UpdateTexture(appState->currentFrameTexture, NULL, appState->rgbFrame->data[0], appState->rgbFrame->linesize[0]);
            frameReady = true;
            SDL_Log("Frame ready for rendering");
        }
    }

    if (frameReady) {
        sync_audio_and_video(appState); // Синхронизация аудио и видео
        SDL_RenderCopy(renderer, appState->currentFrameTexture, NULL, NULL); // Отображение кадра
        frameReady = false;
    } else {
        SDL_Log("No frame available to render");
    }
}*/

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[]) {
    AppState appState = {};

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        SDL_Log("SDL_Init error: %s", SDL_GetError());
        return -1;
    }

    if (TTF_Init() == -1) {
        SDL_Log("TTF_Init error: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    appState.font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 24);
    if (!appState.font) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    appState.window = SDL_CreateWindow("UNO PIECEEEEE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Width, Height, SDL_WINDOW_OPENGL);
    appState.renderer = SDL_CreateRenderer(appState.window, -1, SDL_RENDERER_ACCELERATED);

    if (!appState.window || !appState.renderer) {
        SDL_Log("SDL_CreateWindow or SDL_CreateRenderer error: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    if (!initializeVideoAndAudio(&appState)) {
        SDL_Log("Failed to initialize video");
        cleanup(&appState);
        return -1;
    }

    appState.button.rect = {0, 0, 200, 50};
    appState.button.label = "Click to hack!";
    appState.button.is_clicked = false;
    appState.button.textTexture = CreateTextTexture(appState.renderer, &appState.button, appState.font, {255, 255, 255, 255}, 200, 50);
    UpdateButtonPosition(&appState.button);

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;
                    if (isButtonClicked(mouseX, mouseY, &appState.button)) {
                        handleButtonClick(&appState);
                    }
                }
            }
        }

        SDL_RenderClear(appState.renderer);
        Render_bckgrnd(appState.renderer, &appState);

        drawButton(appState.renderer, &appState.button);
        if (appState.button.textTexture) {
            SDL_Rect textRect = {
                appState.button.rect.x + (appState.button.rect.w - appState.button.text_width) / 2,
                appState.button.rect.y + (appState.button.rect.h - appState.button.text_height) / 2,
                appState.button.text_width,
                appState.button.text_height
            };
            SDL_RenderCopy(appState.renderer, appState.button.textTexture, NULL, &textRect);
        }
        SDL_RenderPresent(appState.renderer);
    }


    cleanup(&appState);
    return 0;
}

bool initializeVideoAndAudio(AppState* appState) {

    appState->formatContext = avformat_alloc_context();
    if (!appState->formatContext) {
        SDL_Log("Failed to allocate format context");
        return false;
    }

    if (avformat_open_input(&appState->formatContext, "Sekai_no_owari.mp4", NULL, NULL) != 0) {
        SDL_Log("Could not open video file");
        return false;
    }

    if (avformat_find_stream_info(appState->formatContext, NULL) < 0) {
        SDL_Log("Could not find stream information");
        return false;
    }

    appState->videoStreamIndex = -1;
    for (unsigned int i = 0; i < appState->formatContext->nb_streams; i++) {
        AVCodecParameters* codecParams = appState->formatContext->streams[i]->codecpar;
        if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO && appState->videoStreamIndex == -1) {
            appState->videoStreamIndex = i;
            appState->videoCodecParams = codecParams;
        }
    }

    if (appState->videoStreamIndex == -1) {
        SDL_Log("Could not find video stream");
        return false;
    }

    appState->videoCodec = avcodec_find_decoder(appState->videoCodecParams->codec_id);
    if (!appState->videoCodec) {
        SDL_Log("Unsupported video codec");
        return false;
    }

    appState->videoCodecContext = avcodec_alloc_context3(appState->videoCodec);
    if (!appState->videoCodecContext) {
        SDL_Log("Failed to allocate video codec context");
        return false;
    }

    if (avcodec_parameters_to_context(appState->videoCodecContext, appState->videoCodecParams) < 0) {
        SDL_Log("Failed to copy video codec parameters");
        return false;
    }

    if (avcodec_open2(appState->videoCodecContext, appState->videoCodec, NULL) < 0) {
        SDL_Log("Failed to open video codec");
        return false;
    }

    appState->swsCtx = sws_getContext(
        appState->videoCodecContext->width, appState->videoCodecContext->height, appState->videoCodecContext->pix_fmt,
        appState->videoCodecContext->width, appState->videoCodecContext->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );
    if (!appState->swsCtx) {
        SDL_Log("Failed to initialize SwsContext");
        return false;
    }

    appState->currentFrameTexture = SDL_CreateTexture(
        appState->renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET,
        appState->videoCodecContext->width, appState->videoCodecContext->height
    );
    if (!appState->currentFrameTexture) {
        SDL_Log("Failed to create SDL texture");
        return false;
    }

    appState->rgbFrame = av_frame_alloc();
    if (!appState->rgbFrame) {
        SDL_Log("Failed to allocate RGB frame");
        return false;
    }

    int rgbBufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, appState->videoCodecContext->width, appState->videoCodecContext->height, 1);
    uint8_t* rgbBuffer = (uint8_t*)av_malloc(rgbBufferSize);
    if (!rgbBuffer) {
        SDL_Log("Failed to allocate RGB buffer");
        return false;
    }

    av_image_fill_arrays(appState->rgbFrame->data, appState->rgbFrame->linesize, rgbBuffer, AV_PIX_FMT_RGB24, appState->videoCodecContext->width, appState->videoCodecContext->height, 1);

    appState->packet = av_packet_alloc();
    if (!appState->packet) {
        SDL_Log("Failed to allocate packet");
        return false;
    }

    appState->frame = av_frame_alloc();
    if (!appState->frame) {
        SDL_Log("Failed to allocate frame");
        return false;
    }

    return true;
}

void cleanup(AppState* appState) {

    av_packet_free(&appState->packet);
    av_frame_free(&appState->frame);
    av_frame_free(&appState->rgbFrame);
    sws_freeContext(appState->swsCtx);
    avcodec_free_context(&appState->videoCodecContext);
    avformat_close_input(&appState->formatContext);

    SDL_DestroyTexture(appState->currentFrameTexture);
    SDL_DestroyRenderer(appState->renderer);
    SDL_DestroyWindow(appState->window);

    if (appState->font) {
        TTF_CloseFont(appState->font);
    }

    TTF_Quit();
    SDL_Quit();
}

bool isButtonClicked(int x, int y, Button* button) {
    return (x >= button->rect.x && x <= (button->rect.x + button->rect.w)) &&
           (y >= button->rect.y && y <= (button->rect.y + button->rect.h));
}

void changeButtonText(SDL_Renderer* renderer, Button* button, TTF_Font* font, const char* newText) {

    if (button->textTexture) {
        SDL_DestroyTexture(button->textTexture);
        button->textTexture = nullptr;
    }

    button->label = newText;

    button->textTexture = CreateTextTexture(renderer, button, font, {255, 255, 255, 255}, button->rect.w, button->rect.h);
}

void handleButtonClick(AppState* appState) {
    SDL_Log("Button clicked! Label: %s", appState->button.label);

    if (!appState->button.is_clicked) {
        const char* newText = NULL;
        appState->button.is_clicked = true;
        int temp = Keygen_inject();
        if (temp)
            newText = "Program successfully hacked!";
        else
            newText = "ERROR: file is already changed or injection was failed";

        changeButtonText(appState->renderer, &appState->button, appState->font, newText);
    }
}

void drawButton(SDL_Renderer* renderer, Button* button) {
    SDL_SetRenderDrawColor(renderer, 0, 128, 255, 255);
    SDL_RenderFillRect(renderer, &button->rect);
}

void UpdateButtonPosition(Button* button) {
    button->rect.x = (Width - button->rect.w) / 2;
    button->rect.y = (Height - button->rect.h) / 2;
}

void RenderButton(SDL_Renderer* renderer, Button* button, SDL_Rect* buttonRect) {
    SDL_SetRenderDrawColor(renderer, 0, 128, 255, 255);
    SDL_RenderFillRect(renderer, buttonRect);

    if (button->textTexture) {
        int textX = buttonRect->x + (buttonRect->w - button->text_width) / 2;
        int textY = buttonRect->y + (buttonRect->h - button->text_height) / 2;

        SDL_Rect textPosition = {textX, textY, button->text_width, button->text_height};
        SDL_RenderCopy(renderer, button->textTexture, NULL, &textPosition);
    } else {
        SDL_Log("No text texture available!");
    }
}

void Render_bckgrnd(SDL_Renderer* renderer, AppState* appState) {
    static bool frameReady = false;
    static int64_t lastPts = 0;
    static Uint32 lastFrameTime = 0;
    static double timeBase = av_q2d(appState->formatContext->streams[appState->videoStreamIndex]->time_base);

    if (!frameReady) {

        while (av_read_frame(appState->formatContext, appState->packet) >= 0) {
            if (appState->packet->stream_index == appState->videoStreamIndex) {
                int ret = avcodec_send_packet(appState->videoCodecContext, appState->packet);
                if (ret < 0) {
                    SDL_Log("Error sending video packet: %s", av_err2str(ret));
                    av_packet_unref(appState->packet);
                    return;
                }
                break;
            }
            av_packet_unref(appState->packet);
        }

        while (true) {
            int ret = avcodec_receive_frame(appState->videoCodecContext, appState->frame);
            if (ret == AVERROR(EAGAIN)) {

                SDL_Log("No frame available (EAGAIN)");
                break;
            } else if (ret == AVERROR_EOF) {

                SDL_Log("End of video stream (EOF)");
                av_seek_frame(appState->formatContext, appState->videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(appState->videoCodecContext);
                break;
            } else if (ret < 0) {

                SDL_Log("Error receiving video frame: %s", av_err2str(ret));
                break;
            }

            int64_t pts = appState->frame->pts;
            if (pts == AV_NOPTS_VALUE) {
                pts = 0;
            }

            if (lastPts != 0) {
                double frameDelay = (pts - lastPts) * timeBase * 1000;
                Uint32 currentTime = SDL_GetTicks();
                Uint32 elapsedTime = currentTime - lastFrameTime;

                SDL_Log("Frame PTS: %ld, Delay: %f ms", pts, frameDelay);

                if (elapsedTime < frameDelay) {
                    SDL_Delay((Uint32)(frameDelay - elapsedTime));
                }
            }

            lastPts = pts;
            lastFrameTime = SDL_GetTicks();

            sws_scale(appState->swsCtx, (uint8_t const* const*)appState->frame->data, appState->frame->linesize,
                      0, appState->videoCodecContext->height,
                      appState->rgbFrame->data, appState->rgbFrame->linesize);

            SDL_UpdateTexture(appState->currentFrameTexture, NULL, appState->rgbFrame->data[0], appState->rgbFrame->linesize[0]);
            frameReady = true;
            SDL_Log("Frame ready for rendering");
        }
    }

    if (frameReady) {
        SDL_RenderCopy(renderer, appState->currentFrameTexture, NULL, NULL);
        frameReady = false;
    } else {
        SDL_Log("No frame available to render");
    }
}

void sync_audio_and_video(AppState* appState) {
    if (appState->frame->pts == AV_NOPTS_VALUE) {
        return;
    }

    double video_time = appState->frame->pts * av_q2d(appState->formatContext->streams[appState->videoStreamIndex]->time_base);

    double audio_time = (double)SDL_GetQueuedAudioSize(appState->audioDevice) /
                        (appState->audioCodecContext->sample_rate *
                         appState->audioCodecContext->ch_layout.nb_channels *
                         sizeof(int16_t));

    if (video_time > audio_time) {
        SDL_Delay((Uint32)((video_time - audio_time) * 1000));
    }
}

void convert_float_to_s16(const float* src, int16_t* dst, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        float sample = src[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        dst[i] = (int16_t)(sample * 32767.0f);
    }
}

SDL_Texture* CreateTextTexture(SDL_Renderer* renderer, Button* button, TTF_Font* font, SDL_Color color, int maxWidth, int maxHeight) {
    if (!font) {
        SDL_Log("ERROR: Font is null");
        return nullptr;
    }

    int fontSize = TTF_FontHeight(font);
    SDL_Texture* textTexture = nullptr;
    SDL_Surface* textSurface = nullptr;

    while (fontSize > 10) {
        TTF_SetFontSize(font, fontSize);
        textSurface = TTF_RenderText_Blended_Wrapped(font, button->label, color, maxWidth);
        if (!textSurface) {
            SDL_Log("ERROR: Failed to create text surface: %s", TTF_GetError());
            return nullptr;
        }

        if (textSurface->w <= maxWidth && textSurface->h <= maxHeight) {
            break;
        }

        SDL_FreeSurface(textSurface);
        fontSize -= 2;
    }

    if (!textSurface) {
        SDL_Log("ERROR: Text is too long even for the smallest font size");
        return nullptr;
    }

    textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_Log("ERROR: Failed to create text texture: %s", SDL_GetError());
    } else {
        button->text_width = textSurface->w;
        button->text_height = textSurface->h;
        SDL_Log("SUCCESS: Text texture created (%d x %d) with font size %d",
                button->text_width, button->text_height, fontSize);
    }

    SDL_FreeSurface(textSurface);
    if (!textTexture) {
        SDL_Log("No text texture available for button: %s", button->label);
    }

    return textTexture;
}

int audio_decode_and_play(AppState* appState) {
    if (!appState || !appState->formatContext || !appState->audioCodecContext || !appState->packet || !appState->frame) {
        SDL_Log("Invalid appState or uninitialized pointers");
        return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        SDL_Log("Failed to allocate packet");
        return -1;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        SDL_Log("Failed to allocate frame");
        av_packet_free(&pkt);
        return -1;
    }

    while (av_read_frame(appState->formatContext, pkt) >= 0) {
        if (pkt->stream_index == appState->audioStreamIndex) {
            int ret = avcodec_send_packet(appState->audioCodecContext, pkt);
            if (ret < 0) {
                SDL_Log("Error sending audio packet: %s", av_err2str(ret));
                av_packet_unref(pkt);
                continue;
            }

            while (true) {
                ret = avcodec_receive_frame(appState->audioCodecContext, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    SDL_Log("Error receiving audio frame: %s", av_err2str(ret));
                    break;
                }

                int data_size = av_samples_get_buffer_size(
                    NULL,
                    appState->audioCodecContext->ch_layout.nb_channels,
                    frame->nb_samples,
                    appState->audioCodecContext->sample_fmt,
                    1
                );

                if (data_size > 0) {

                    if (SDL_GetQueuedAudioSize(appState->audioDevice) > 1024 * 1024) {
                        SDL_ClearQueuedAudio(appState->audioDevice);
                        SDL_Log("Cleared audio queue to prevent overflow");
                    }

                    if (frame->format == AV_SAMPLE_FMT_FLTP || frame->format == AV_SAMPLE_FMT_FLT) {
                        // Преобразуем float в s16
                        int16_t* s16_buffer = (int16_t*)malloc(data_size * sizeof(int16_t));
                        convert_float_to_s16((const float*)frame->data[0], s16_buffer, frame->nb_samples * appState->audioCodecContext->ch_layout.nb_channels);
                        SDL_QueueAudio(appState->audioDevice, s16_buffer, data_size * sizeof(int16_t));
                        free(s16_buffer);
                    } else if (frame->format == AV_SAMPLE_FMT_S16P || frame->format == AV_SAMPLE_FMT_S16) {

                        SDL_QueueAudio(appState->audioDevice, frame->data[0], data_size);
                    } else {
                        SDL_Log("Unsupported audio format: %d", frame->format);
                    }
                } else {
                    SDL_Log("Audio data size is 0");
                }
            }
        }
        av_packet_unref(pkt);
    }

    if (av_read_frame(appState->formatContext, pkt) == AVERROR_EOF) {
        SDL_Log("End of file reached. Rewinding...");

        av_seek_frame(appState->formatContext, appState->audioStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(appState->audioCodecContext);
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);
    return 0;
}
