//
//  messagepad_SDL.cpp
//  MessagePad_SDL
//
//  Created by Matthias Melcher on 7/9/24.
//  Copyright © 2024 Newton Research. All rights reserved.
//

// Platform/MPControlle.mm
// ErrorString.m

#include "messagepad_SDL.h"

#include <iostream>
#include <algorithm>
#include <thread>

#include <SDL.h>

#include "NewtonTypes.h"
#include "UserGlobals.h"


extern "C" void ResetNewton(void); // boot.cc

#define kTabScale 8.0
extern "C" void    PenDown(float inX, float inY);
extern "C" void    PenMoved(float inX, float inY);
extern "C" void    PenUp(void);

SDL_Window* gSDLWindow = nullptr;
SDL_Surface* gSDLScreen = nullptr;
SDL_Surface* gSDLPixels = nullptr;

#define kIntSourceTimer            0x0020
#define kIntSourceScheduler    0x0040
extern void    DispatchFakeInterrupt(uint32_t inSourceMask);
extern ULong gCPSR;
extern ULong gPendingInterrupt;


Uint32 scheduler_cb(Uint32, void*) {
    if (FLAGTEST(gCPSR, kIRQDisable)) {
        gPendingInterrupt |= kIntSourceScheduler;
    } else {
        // handle the scheduler interrupt
        putchar('>');
        DispatchFakeInterrupt(kIntSourceScheduler);
        //            GetPlatformDriver()->timerInterruptHandler();
    }
   return 500; // 20?
}


int main(int argc, char* argv[])
{
    // -lSDL2_ttf
    // TTF_Font* Sans = TTF_OpenFont("Sans.ttf", 24);
    // TTF_RenderText_Solid(Sans, "put your text here", White); ??
    // SDL_RenderCopy(renderer, Message, NULL, &Message_rect);

    // Unused argc, argv
    (void) argc;
    (void) argv;

    int width = 320;
    int height = 480;

    SDL_Init(SDL_INIT_VIDEO |  SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_EVENTS);
    gSDLWindow = SDL_CreateWindow("MessagePad", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    gSDLScreen = SDL_GetWindowSurface(gSDLWindow);
    gSDLPixels = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    // SDL_MUSTLOCK(pixels) == 0 ?
    
//    unsigned int t1 = SDL_GetTicks();

    std::thread gNewtonThread(ResetNewton);

    SDL_AddTimer(500, scheduler_cb, nullptr);

    float pos = 0;
    bool done = false;

    while (!done)
    {
        SDL_Event ev;
//        if (SDL_PollEvent(&ev))
        if (SDL_WaitEvent(&ev))
        {
            switch (ev.type) {
                case SDL_QUIT:
                    done = true;
                    // TODO: tell gNewtonThread to shut down
                    break;
                case SDL_USEREVENT:
                    if (((SDL_UserEvent&)ev).code == 1) {
                        // about 30 times a second if screen contents changed
                        SDL_BlitSurface(gSDLPixels, NULL, gSDLScreen, NULL);
                        SDL_UpdateWindowSurface(gSDLWindow);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    PenDown(((SDL_MouseButtonEvent&)ev).x * kTabScale,
                            ((SDL_MouseButtonEvent&)ev).y * kTabScale);
                    break;
                case SDL_MOUSEBUTTONUP:
                    PenUp();
                    break;
                case SDL_MOUSEMOTION:
                    if ((((SDL_MouseMotionEvent&)ev).state & 0x07) == SDL_BUTTON_LMASK) {
                        PenMoved(((SDL_MouseMotionEvent&)ev).x * kTabScale,
                                 ((SDL_MouseMotionEvent&)ev).y * kTabScale);
                    }
                    break;
            }
        }
    }

    SDL_FreeSurface(gSDLPixels);
    SDL_DestroyWindowSurface(gSDLWindow);
    SDL_DestroyWindow(gSDLWindow);
    SDL_Quit();

    // TODO: quit all other threads somehow
    return 0;
}
