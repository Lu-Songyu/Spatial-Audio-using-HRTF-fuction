// HRTF Spatialized Audio
//
// See README.md for more information
//
// Written by Ryan Huffman <ryanhuffman@gmail.com>


#include "SDL2/include/SDL.h"
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "kiss_fft.h"

#include "hrtf.h"

const char HRTF_FILE_FORMAT_MIT[] = "mit/elev%d/H%de%03da.wav";
const char HRTF_FILE_FORMAT_CIPIC[] = "cipic/subject%03d/e%da%03d.wav";
const char AUDIO_FILE[] = "./beep.wav";
const char BEE_FILE[] = "./fail-buzzer-01.wav";
const char StarWar_FILE[] = "./StarWars3.wav";
const char Train_FILE[] = "./steam-train.wav";// /train-whistle-01.wav";

const float FPS = 60.0f;
const float FRAME_TIME = 16.6666667f;   // 1000 / FPS

const int NUM_SAMPLES_PER_FILL = 512;
const int SAMPLE_SIZE = sizeof(float);
const int FFT_POINTS = 512;             // NUM_SAMPLES_PER_FILL

const int SAMPLE_RATE = 44100;

// Configs for forward and inverse FFT
kiss_fft_cfg cfg_forward;
kiss_fft_cfg cfg_inverse;

// stores which subject HRTF data being used
int subject = 0;

// Number of samples stored in the audio file
int total_samples = 0;
double accuracy = 100.0;
int correct = 0;
int totalGuess = 0;
// starting and ending azimuths
bool testMode = false;
int azimuth = 0;
int start = 0, finish = 360;
int userC;
int jumpC = 0;
// FFT storage for convolution with HRTFs during playback
kiss_fft_cpx* audio_kiss_buf;    // Audio data, time domain
kiss_fft_cpx* audio_kiss_freq;   // Audio data, stores a single sample, freq domain
kiss_fft_cpx* audio_kiss_freq_l; // Audio sample multiplied by HRTF, left ear
kiss_fft_cpx* audio_kiss_freq_r; // Audio sample multiplied by HRTF, right ear
kiss_fft_cpx* audio_kiss_time_l; // Final, convolved audio sample, left ear
kiss_fft_cpx* audio_kiss_time_r; // Final, convolved audio sample, right ear


// HRTF data for each point on the horizontal plane (0 ... 180)
const int AZIMUTH_CNT = 37;
const int AZIMUTH_INCREMENT_DEGREES = 5;
hrtf_data hrtfs[37];                        // AZIMUTH CNT

typedef struct {
    SDL_Rect draw_rect;    // dimensions of button
    struct {
        Uint8 r, g, b, a;
    } colour;

    bool pressed;
} button_t;

static void button_process_event(button_t *btn, const SDL_Event *ev) {
    // react on mouse click within button rectangle by setting 'pressed'
    if(ev->type == SDL_MOUSEBUTTONDOWN) {
        if(ev->button.button == SDL_BUTTON_LEFT &&
                ev->button.x >= btn->draw_rect.x &&
                ev->button.x <= (btn->draw_rect.x + btn->draw_rect.w) &&
                ev->button.y >= btn->draw_rect.y &&
                ev->button.y <= (btn->draw_rect.y + btn->draw_rect.h)) {
            btn->pressed = true;
        }
    }
}

// buf_len should be the number of data point in the stereo `buf`
// Each sample should have 2 data points; 1 for each ear
void init_hrtf_data(hrtf_data* data, float* buf, int buf_len, int azimuth, int elevation) {
    // Initialize properties
    data->azimuth = azimuth;
    data->elevation = elevation;

    // Not really necessary to hold on to the HRIR data
    data->hrir_l = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);
    data->hrir_r = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);

    data->hrtf_l = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);
    data->hrtf_r = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);

    for (int i = 0; i < NUM_SAMPLES_PER_FILL; i++) {
        if (i < buf_len / 2) {
            data->hrir_l[i].r = buf[i * 2];
            data->hrir_r[i].r = buf[(i * 2) + 1];
        } else {
            data->hrir_l[i].r = 0;
            data->hrir_r[i].r = 0;
        }
        data->hrir_l[i].i = 0;
        data->hrir_r[i].i = 0;
    }

    kiss_fft(cfg_forward, data->hrir_l, data->hrtf_l);
    kiss_fft(cfg_forward, data->hrir_r, data->hrtf_r);
}

void free_hrtf_data(hrtf_data* data) {
    free(data->hrir_l);
    free(data->hrir_r);
    free(data->hrtf_l);
    free(data->hrtf_r);
}

// udata: user data
// stream: stream to copy into
// len: number of bytes to copy into stream
void fill_audio(void* udata, Uint8* stream, int len ) {
    
    static int sample = 0;
   
    if(azimuth < start ) {
        azimuth = start;
    }
    

    bool swap = false;
    static bool reverse = false;

    if (sample >= total_samples) {
        // azimuth += AZIMUTH_INCREMENT_DEGREES;
        if(start != finish) {
        if(reverse) {
            azimuth -= AZIMUTH_INCREMENT_DEGREES;
            if(azimuth < start) {
                reverse = false;
                azimuth += AZIMUTH_INCREMENT_DEGREES;
                azimuth += AZIMUTH_INCREMENT_DEGREES;
            }
        } else {
            azimuth += AZIMUTH_INCREMENT_DEGREES;
            if(azimuth > finish) {
                reverse = true;
                azimuth -= AZIMUTH_INCREMENT_DEGREES;
                azimuth -= AZIMUTH_INCREMENT_DEGREES;
            }
        }
        // No reverse for standard path
        if(userC == 1){

            reverse = false;
            azimuth %= 360;
        }

        }
        
        if(jumpC == 1){ // increment by 10
            if(reverse == false){
                    azimuth += AZIMUTH_INCREMENT_DEGREES;

                }else{
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                }
        }
        else if(jumpC == 2){ // increment by 20
            if(reverse == false){
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                }else{
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                }

        }
        else if(jumpC == 3){    // increment by 30
            if(reverse == false){
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                }else{
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                }

        }
        else if(jumpC == 4){    // increment by 40
            if(reverse == false){
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                }else{
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                }

        }
           
        sample = 0;

        
        if(testMode == false){
             printf("Azimuth: %d\n", azimuth);
        }
        // only print azimuth value if not testing
    }

    int num_samples = len / SAMPLE_SIZE / 2;
    if (total_samples - sample < num_samples) {
        num_samples = total_samples - sample;
    }

    int azimuth_idx = azimuth / AZIMUTH_INCREMENT_DEGREES;
    
    // Because the HRIR recordings are only from 0-180, we swap them when > 180
    if(!subject) {
        if (azimuth > 180) {
            swap = true;
            azimuth_idx = 35 - (azimuth_idx % 37);
        }
    }
    
    hrtf_data* data = &hrtfs[azimuth_idx];

    kiss_fft_cpx* hrtf_l = data->hrtf_l;
    kiss_fft_cpx* hrtf_r = data->hrtf_r;

    // Calculate DFT of sample
    kiss_fft(cfg_forward, audio_kiss_buf + sample, audio_kiss_freq);

    // Apply HRTF
    for (int i = 0; i < num_samples; i++) {
        audio_kiss_freq_l[i].r = (audio_kiss_freq[i].r * hrtf_l->r) - (audio_kiss_freq[i].i * hrtf_l->i);
        audio_kiss_freq_l[i].i = (audio_kiss_freq[i].r * hrtf_l->i) + (audio_kiss_freq[i].i * hrtf_l->r);
    }

    for (int i = 0; i < num_samples; i++) {
        audio_kiss_freq_r[i].r = (audio_kiss_freq[i].r * hrtf_r->r) - (audio_kiss_freq[i].i * hrtf_r->i);
        audio_kiss_freq_r[i].i = (audio_kiss_freq[i].r * hrtf_r->i) + (audio_kiss_freq[i].i * hrtf_r->r);
    }

    // Run reverse FFT to get audio in time domain
    kiss_fft(cfg_inverse, audio_kiss_freq_l, audio_kiss_time_l);
    kiss_fft(cfg_inverse, audio_kiss_freq_r, audio_kiss_time_r);

    // Copy data to stream
    for (int i = 0; i < num_samples; i++) {
        if (swap) {
            ((float*)stream)[i * 2] = audio_kiss_time_r[i].r / FFT_POINTS;
            ((float*)stream)[i * 2 + 1] = audio_kiss_time_l[i].r / FFT_POINTS;
        } else {
            ((float*)stream)[i * 2] = audio_kiss_time_l[i].r / FFT_POINTS;
            ((float*)stream)[i * 2 + 1] = audio_kiss_time_r[i].r / FFT_POINTS;
        }
    }

    sample += num_samples;
}

void print_audio_spec(SDL_AudioSpec* spec) {
    printf("\tFrequency: %u\n", spec->freq);
    const char* sformat;
    switch (spec->format) {
        case AUDIO_S8:
            sformat = S_AUDIO_S8;
            break;
        case AUDIO_U8:
            sformat = S_AUDIO_U8;
            break;

        case AUDIO_S16LSB:
            sformat = S_AUDIO_S16LSB;
            break;
        case AUDIO_S16MSB:
            sformat = S_AUDIO_S16MSB;
            break;

        case AUDIO_U16LSB:
            sformat = S_AUDIO_U16LSB;
            break;
        case AUDIO_U16MSB:
            sformat = S_AUDIO_U16MSB;
            break;

        case AUDIO_S32LSB:
            sformat = S_AUDIO_S32LSB;
            break;
        case AUDIO_S32MSB:
            sformat = S_AUDIO_S32MSB;
            break;

        case AUDIO_F32LSB:
            sformat = S_AUDIO_F32LSB;
            break;
        case AUDIO_F32MSB:
            sformat = S_AUDIO_F32MSB;
            break;

        default:
            sformat = S_AUDIO_UNKNOWN;
            break;
    }
    printf("\tFormat: %s\n", sformat);
    printf("\tChannels: %hhu\n", spec->channels);
    printf("\tSilence: %hhu\n", spec->silence);
    printf("\tSamples: %hu\n", spec->samples);
    printf("\tBuffer Size: %u\n", spec->size);
}

SDL_AudioDeviceID MakeAudio(int begin, int end, int sound, int choice, int jump){
    SDL_AudioSpec obtained_audio_spec;
    SDL_AudioSpec desired_audio_spec;
    SDL_AudioCVT audio_cvt;
    SDL_AudioCVT hrtf_audio_cvt;
    int numDevices, num = 0;

    // Audio output format
    desired_audio_spec.freq = SAMPLE_RATE;
    desired_audio_spec.format = AUDIO_F32;
    desired_audio_spec.channels = 2;
    desired_audio_spec.samples = NUM_SAMPLES_PER_FILL;
    desired_audio_spec.callback = fill_audio;
    desired_audio_spec.userdata = NULL;

    numDevices = SDL_GetNumAudioDevices(0);
    printf("Device count: %d\n", numDevices);
    const char* device_name[numDevices]; 
    if(numDevices > 1) {
        printf("Multiple devices detected: \n");
        for(int i =0; i < numDevices; i ++) {
            device_name[i] = SDL_GetAudioDeviceName(i, 0);
            printf("Press %d for %s\n", i, device_name[i]);
        }
        scanf("%d", &num);
    }
    printf("Device name: %s\n", device_name[num]);

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(device_name[num], 0, &desired_audio_spec, &obtained_audio_spec, 0);

    printf("Desired Audio Spec:\n");
    print_audio_spec(&desired_audio_spec);

    printf("Obtained Audio Spec:\n");
    print_audio_spec(&obtained_audio_spec);

    SDL_AudioSpec* file_audio_spec;
    Uint8* audio_buf;
    Uint32 audio_len;
    Uint8* audio_pos;
    // Open audio file
    file_audio_spec = malloc(sizeof(SDL_AudioSpec));
    if (!file_audio_spec) {
        printf("Failed to allocate audio spec for wav file");
        return 1;
    }

    // specified audio file is used
    if(sound == 3) {
        if (!SDL_LoadWAV(BEE_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", BEE_FILE);
            SDL_Quit();
           return 1;
        }
    } else if(sound == 1){
        if (!SDL_LoadWAV(StarWar_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", StarWar_FILE);
            SDL_Quit();
           return 1;
        }
    }else if(sound == 2){
        if (!SDL_LoadWAV(Train_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", Train_FILE);
            SDL_Quit();
           return 1;
        }
    }
     else {
        if (!SDL_LoadWAV(AUDIO_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", AUDIO_FILE);
            SDL_Quit();
           return 1;
        }
    }

    printf("Wav Spec:\n");
    print_audio_spec(file_audio_spec);


    // Use mono, the audio will be stereo when the HRTFs are applied
    SDL_BuildAudioCVT(&audio_cvt,
                      file_audio_spec->format, file_audio_spec->channels, file_audio_spec->freq,
                      obtained_audio_spec.format, 1, obtained_audio_spec.freq);

    printf("About to convert wav\n");
    audio_cvt.buf = malloc(audio_len * audio_cvt.len_mult);
    audio_cvt.len = audio_len;
    memcpy(audio_cvt.buf, audio_buf, audio_len);
    SDL_ConvertAudio(&audio_cvt);
    printf("Converted wav\n");

    free(audio_buf);
    audio_buf = audio_cvt.buf;
    audio_pos = audio_buf;
    audio_len = audio_cvt.len_cvt;

    cfg_forward = kiss_fft_alloc(NUM_SAMPLES_PER_FILL, 0, NULL, NULL);
    cfg_inverse = kiss_fft_alloc(NUM_SAMPLES_PER_FILL, 1, NULL, NULL);

    // This will store the entire audio file
    int num_audio_samples = audio_len / sizeof(float);
    // 0-pad up to the end
    int padded_len = ((num_audio_samples - 1) / NUM_SAMPLES_PER_FILL) * NUM_SAMPLES_PER_FILL;
    audio_kiss_buf = malloc(sizeof(kiss_fft_cpx) * padded_len);
    memset(audio_kiss_buf, 0, padded_len);
    total_samples = padded_len;

    const int FFT_SIZE = sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL;

    audio_kiss_freq = malloc(FFT_SIZE);
    audio_kiss_freq_l = malloc(FFT_SIZE);
    audio_kiss_freq_r = malloc(FFT_SIZE);
    audio_kiss_time_l = malloc(FFT_SIZE);
    audio_kiss_time_r = malloc(FFT_SIZE);

    memset(audio_kiss_freq, 0, FFT_SIZE);
    memset(audio_kiss_freq_l, 0, FFT_SIZE);
    memset(audio_kiss_freq_r, 0, FFT_SIZE);
    memset(audio_kiss_time_l, 0, FFT_SIZE);
    memset(audio_kiss_time_r, 0, FFT_SIZE);

    for (int i = 0; (i * SAMPLE_SIZE) < audio_len; i++) {
        int idx = i;
        audio_kiss_buf[idx].r = ((float*)audio_buf)[i];
        audio_kiss_buf[idx].i = 0;
    }
    int limit = 72;
    if(!subject) {
        limit = 37;
    }

    for (int azimuth = 0; azimuth < limit; azimuth++) {
        char filename[100];
        if(!subject){
            sprintf(filename, HRTF_FILE_FORMAT_MIT, 0, 0, azimuth * 5);
            printf("Loading: %s\n", filename, 0, 0, azimuth * 5);
        } else {
            sprintf(filename, HRTF_FILE_FORMAT_CIPIC, subject, 0, azimuth * 5);
            printf("Loading: %s\n", filename, subject, 0, azimuth * 5);
        }

        SDL_AudioSpec audiofile_spec;
        Uint8* hrtf_buf;
        Uint32 hrtf_len;

        if (!SDL_LoadWAV(filename, &audiofile_spec, &hrtf_buf, &hrtf_len)) {
            printf("Could not load hrtf file (%s): %s\n", filename, SDL_GetError());
            SDL_Quit();
            return 1;
        }

        SDL_BuildAudioCVT(&hrtf_audio_cvt,
                          audiofile_spec.format, audiofile_spec.channels, audiofile_spec.freq,
                          AUDIO_F32LSB, 2, audiofile_spec.freq);

        hrtf_audio_cvt.buf = malloc(hrtf_len* hrtf_audio_cvt.len_mult);
        hrtf_audio_cvt.len = hrtf_len;
        memcpy(hrtf_audio_cvt.buf, hrtf_buf, hrtf_len);
        SDL_ConvertAudio(&hrtf_audio_cvt);

        init_hrtf_data(&hrtfs[azimuth], (float*)hrtf_audio_cvt.buf,
                       hrtf_len / SAMPLE_SIZE, azimuth * AZIMUTH_INCREMENT_DEGREES, 0);

        free(hrtf_buf);
    }
    return audio_device;
}

char * toArray(int number)
    {
        int n = log10(number) + 1;
        int i;
      char *numberArray = calloc(n, sizeof(char));
        for ( i = 0; i < n; ++i, number /= 10 )
        {
            numberArray[i] = number % 10;
        }
        return numberArray;
    }

void GUI(int begin, int end, int sound, int choice, int jump, SDL_AudioDeviceID audio_device){
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window *window;
    SDL_Surface *windowSurface;
    
    //SDL_StartTextInput();
    SDL_Surface *intro;
    SDL_Surface *menu;
    SDL_Surface *chooseP;
    SDL_Surface *chooseA;
    SDL_Surface *chooseEffect;
    SDL_Surface *InputA;
    SDL_Surface *testing;
    SDL_Surface *customize;
   
    SDL_Surface *currentImage;

    
    window = SDL_CreateWindow("HRTF", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 440, SDL_WINDOW_SHOWN);
    windowSurface = SDL_GetWindowSurface(window);

    intro = SDL_LoadBMP("test1.bmp");
    testing = SDL_LoadBMP("test2.bmp");
    menu = SDL_LoadBMP("Menu.bmp");
    chooseP = SDL_LoadBMP("choosePath.bmp");
    chooseA = SDL_LoadBMP("chooseA.bmp");
    chooseEffect = SDL_LoadBMP("SoundE.bmp");
    InputA = SDL_LoadBMP("azimuth.bmp");
    customize = SDL_LoadBMP("Custome.bmp");
    currentImage = intro;

    /*SDL_Renderer* renderer = NULL;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(!renderer) {   // renderer creation may fail too
        fprintf(stderr, "create renderer failed: %s\n", SDL_GetError());
        return 1;
    }*/

    button_t start_button = {
        .colour = { .r = 255, .g = 255, .b = 255, .a = 255, },
        .draw_rect = { .x = 160, .y = 400, .w = 320, .h = 32 },
    };
    // buttons for testing
    button_t left_button = {
        .draw_rect = { .x = 17, .y = 94, .w = 39, .h = 32 },
    };
    button_t backleft_button = {
        .draw_rect = { .x = 17, .y = 290, .w = 80, .h = 32 },
    };
    button_t back_button = {
        .draw_rect = { .x = 280, .y = 290, .w = 65, .h = 32 },
    };
    button_t backright_button = {
        .draw_rect = { .x = 525, .y = 290, .w = 97, .h = 32 },
    };
    button_t right_button = {
        .draw_rect = { .x = 560, .y = 94, .w = 53, .h = 32 },
    };
    //SDL_SetRenderDrawColor(renderer, start_button.colour.r, start_button.colour.g, start_button.colour.b, start_button.colour.a);
    //SDL_RenderFillRect(renderer, &start_button.draw_rect);
    SDL_AudioDeviceID device = device;
    bool isRunning = true;
    SDL_Event ev;
    int temp;
    bool playing = false;
    
    int var = -1; // -1 for intro, 0 for menu, 1 for choose path, 2 for choose audio, 3 for choose sound effect, 5 for testing
    char str[100];  // initalize a temp to store azimuth input
    char endA[4];
    char startA[4];

    char accA[10];
    memset(str, 0, sizeof str);
    //SDL_StartTextInput();

    while(isRunning){

        while (SDL_PollEvent(&ev) !=0)
        {
            if(ev.type == SDL_MOUSEBUTTONUP) {
                //printf("Button Pressed!\n");
            }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_SPACE) {
                 if(playing == true){
                     SDL_PauseAudioDevice(audio_device, 1);
                     playing = false;
                 }
                 else if(playing == false){
                     SDL_PauseAudioDevice(audio_device, 0);
                     playing = true;
                 }
            }
            button_process_event(&start_button, &ev); 
            button_process_event(&back_button, &ev);  
            button_process_event(&backleft_button, &ev);  
            button_process_event(&backright_button, &ev);  
            button_process_event(&left_button, &ev);  
            button_process_event(&right_button, &ev);
            if( var == 5 ){
                if (left_button.pressed || right_button.pressed || backleft_button.pressed ||back_button.pressed || backright_button.pressed){
                    // stop the audio when button is pressed
                    if(left_button.pressed){
                        printf("Left Button Pressed! \n");
                        if(azimuth > 240 && azimuth < 270){
                                totalGuess++;
                                correct++;
                        }else{
                            totalGuess++;
                        }
                    }
                    if(right_button.pressed){
                        printf("Right Button Pressed! \n");
                        if(azimuth > 90 && azimuth < 120){
                                totalGuess++;
                                correct++;
                        }else{
                            totalGuess++;
                        }
                    }
                    if(backleft_button.pressed){
                        printf("Back Left Button Pressed! \n");
                        if(azimuth > 210 && azimuth < 240){
                                totalGuess++;
                                correct++;
                        }else{
                            totalGuess++;
                        }
                    }
                    if(backright_button.pressed){
                        printf("back Right Button Pressed! \n");
                        if(azimuth > 120 && azimuth < 150){
                                totalGuess++;
                                correct++;
                        }else{
                            totalGuess++;
                        }
                    }
                    if(back_button.pressed){
                        printf("back Button Pressed! \n");
                        if(azimuth > 175 && azimuth < 195){
                                totalGuess++;
                                correct++;
                        }else{
                            totalGuess++;
                        }
                    }
                    
                    back_button.pressed = false;
                    backleft_button.pressed = false;
                    backright_button.pressed = false;
                    left_button.pressed = false;
                    right_button.pressed = false;
                }
            }    
            if(start_button.pressed) {
                if(!playing) {
                audio_device = MakeAudio(begin, end, sound, choice, jump);
                SDL_PauseAudioDevice(audio_device, 0);
                playing = true;
                start_button.pressed = false;
                } else {
                    SDL_PauseAudioDevice(audio_device, 1);
                    playing = false;
                }
            }
            switch (ev.type)
            {
            case SDL_QUIT:
                //return 0;
                 isRunning = false;
                 //running = false;
                break;
            case SDL_MOUSEWHEEL: //wheel up and down only available in menu page
                if(ev.wheel.y > 0 && var == 0){
                    currentImage = intro;
                    var = -1;
                }
                else if(ev.wheel.y < 0 && var == -1){
                    currentImage = menu;
                    var = 0;              
                }      
                break;
            case SDL_KEYDOWN:
                /*if(ev.key.keysym.sym == SDLK_RETURN) {
                    isRunning = false;
                }*/
                if(var == 4){ // input customized azimuth
                    switch (ev.key.keysym.sym){
                        case SDLK_0:
                            strcat(str, "0");
                            break;
                        case SDLK_1:
                            strcat(str, "1");
                            break;
                        case SDLK_2:
                            strcat(str, "2");
                            break;
                        case SDLK_3:
                            strcat(str, "3");
                            break;
                        case SDLK_4:
                            strcat(str, "4");
                            break;
                        case SDLK_5:
                            strcat(str, "5");
                            break;
                        case SDLK_6:
                            strcat(str, "6");
                            break;
                        case SDLK_7:
                            strcat(str, "7");
                            break;
                        case SDLK_8:
                            strcat(str, "8");
                            break;
                        case SDLK_9:
                            strcat(str, "9");
                            break;
                        case SDLK_SPACE:    // press space to restart the whole process
                            start = 0;
                            finish = 360;
                            break;
                        case SDLK_RETURN:
                            temp = 100*(str[0]- '0')+ 10*(str[1] - '0')+ (str[2] - '0');
                            printf("%d\n", temp);
                            if(start != 0) {
                                strcpy(endA, str);
                                if(temp > 360){
                                    finish = 360;
                                }else
                                {
                                    finish = temp;
                                }
                            } else {
                                strcpy(startA, str);
                                if(temp < 0){
                                    start = 0;
                                }else
                                {
                                    start = temp;
                                }
                            }
                            memset(str, 0, sizeof str);
                            break;
                        case SDLK_ESCAPE:
                            strcat(str, "Starting azimuth is: ");
                            strcat(str, startA);
                            strcat(str, "\n");
                            strcat(str, "Ending azimuth is: ");
                            strcat(str, endA);
                            SDL_ShowSimpleMessageBox(0, "Azimuth", str, window);
                            currentImage = menu;
                            var = 0;
                            memset(str, 0, sizeof str);
                            break;

                    }
                } else if(var == 0){
                    switch (ev.key.keysym.sym)
                    {
                    case SDLK_1:
                        currentImage = chooseP; 
                        var = 1;
                        break;
                    case SDLK_2:
                        currentImage = chooseA;
                        var = 2;
                        break;
                    case SDLK_3:
                        currentImage = chooseEffect;
                        var = 3;
                        break;
                    case SDLK_RETURN:
                        audio_device = MakeAudio(begin, end, sound, choice, jump);
                        SDL_PauseAudioDevice(audio_device, 0);
                        playing = true;
                        break;
                    case SDLK_TAB:
                        SDL_PauseAudioDevice(audio_device, 1);
                        SDL_DestroyWindow(window); 
                    case SDLK_4:
                        currentImage = testing;
                        var = 5;
                        break;
                    case SDLK_5:
                        currentImage = customize;
                        var = 6;
                        break;
                    }
                } else if(var == 5){   // testing page (WIP)
                    testMode = true;
                    switch (ev.key.keysym.sym)
                    {
                    case SDLK_RETURN:
                        start = 90;
                        finish = 270;
                        audio_device = MakeAudio(begin, end, sound, choice, jump);
                        SDL_PauseAudioDevice(audio_device, 0);
                        playing = true;
                        break;
                    case SDLK_ESCAPE:
                         SDL_PauseAudioDevice(audio_device, 1);
                        playing = false;  
                        currentImage = menu;
                        var = 0;
                        accuracy = (double)correct/totalGuess;
                        accuracy = 100*accuracy;
                        sprintf(accA,"%ld", (int)accuracy);
                        strcat(str, "Accuracy: ");
                        strcat(str, accA);
                        strcat(str, "%");
                        SDL_ShowSimpleMessageBox(0, "Testing", str, window);
                        memset(str, 0, sizeof str);
                        break;

                    }

                } else if(var == 6) {
                    switch (ev.key.keysym.sym){
                        case SDLK_0:
                            strcat(str, "0");
                            break;
                        case SDLK_1:
                            strcat(str, "1");
                            break;
                        case SDLK_2:
                            strcat(str, "2");
                            break;
                        case SDLK_3:
                            strcat(str, "3");
                            break;
                        case SDLK_4:
                            strcat(str, "4");
                            break;
                        case SDLK_5:
                            strcat(str, "5");
                            break;
                        case SDLK_6:
                            strcat(str, "6");
                            break;
                        case SDLK_7:
                            strcat(str, "7");
                            break;
                        case SDLK_8:
                            strcat(str, "8");
                            break;
                        case SDLK_9:
                            strcat(str, "9");
                            break;
                        case SDLK_SPACE:    // press space to restart the whole process
                            subject = 0;
                            break;
                        case SDLK_RETURN:
                            temp = 100*(str[0]- '0')+ 10*(str[1] - '0')+ (str[2] - '0');
                            printf("%d\n", temp);
                            strcpy(endA, str);
                            subject = temp;
                            memset(str, 0, sizeof str);
                            break;
                        case SDLK_ESCAPE:
                            strcat(str, "Subject selected is: ");
                            strcat(str, endA);
                            strcat(str, "\n");
                            SDL_ShowSimpleMessageBox(0, "Azimuth", str, window);
                            currentImage = menu;
                            var = 0;
                            memset(str, 0, sizeof str);
                            break;

                    }
                } else if(var == 1){
                    switch (ev.key.keysym.sym)
                    {
                    case SDLK_0:
                        choice = 1;
                        
                        break;
                    case SDLK_1:
                        choice = 2;
                        break;
                    case SDLK_RETURN:
                        currentImage = InputA;
                        var = 4;
                        break;
                    case SDLK_ESCAPE:
                        currentImage = menu;
                        var = 0;
                        break;
                    }
                } else if(var == 2){
                    switch (ev.key.keysym.sym)
                    {
                    case SDLK_0:
                        sound = 0;
                        strcpy(str, "Audio choice: beep");  // use strcpy instead of strcat to avoid adding undesigned string to str.
                        break;
                    case SDLK_1:
                        sound = 1;
                        strcpy(str, "Audio choice: star war");
                        break;
                    case SDLK_2:
                        sound = 2;
                        strcpy(str, "Audio choice: train");
                        break;
                    case SDLK_3:
                        sound = 3;
                        strcpy(str, "Audio choice: bee");
                        break;
                    case SDLK_ESCAPE:
                        SDL_ShowSimpleMessageBox(0, "Audio", str, window);
                        currentImage = menu;
                        var = 0;
                        memset(str, 0, sizeof str);
                        break;
                    }
                } else if(var == 3){
                    switch (ev.key.keysym.sym)
                    {
                    case SDLK_0:
                        jumpC = 0;
                        strcpy(str, "Speed Level：Default");
                        break;
                    case SDLK_1:
                        jumpC = 1;
                        strcpy(str, "Speed Level： 1");
                        break;
                    case SDLK_2:
                        jumpC = 2;
                        strcpy(str, "Speed Level： 2");
                        break;
                    case SDLK_3:
                        jumpC = 3;
                        strcpy(str, "Speed Level： 3");
                        break;
                    case SDLK_4:
                        jumpC = 4;
                        strcpy(str, "Speed Level： 4");
                        break;
                    case SDLK_ESCAPE:
                        SDL_ShowSimpleMessageBox(0, "Speed", str, window);
                        currentImage = menu;
                        var = 0;
                        memset(str, 0, sizeof str);
                        break;
                    }
                }
                
                break;
                

            }
            
        }
        

        SDL_BlitSurface(currentImage, NULL, windowSurface, NULL);
        SDL_UpdateWindowSurface(window);
    }
    return;
}
//Uint8* audio_buf, Uint32 audio_len, SDL_AudioSpec* file_audio_spec, Uint8* audio_pos

int main(int argc, char* argv[]) {
    int begin = 0,
        end = 360, 
        sound = 0,
        choice = 0,
        jump = 0;
        
    SDL_AudioDeviceID device;
    GUI(begin, end, sound, choice, jump, device);
    
    // Cleanup
    SDL_CloseAudio();
    for (int i = 0; i < AZIMUTH_CNT; i++) {
        free_hrtf_data(&hrtfs[i]);
    }
    
    SDL_Quit();

    return 0;
}