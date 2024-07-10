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

extern "C" void ResetNewton(void); // boot.cc

#define kTabScale 8.0
extern "C" void    PenDown(float inX, float inY);
extern "C" void    PenMoved(float inX, float inY);
extern "C" void    PenUp(void);

SDL_Window* gSDLWindow = nullptr;
SDL_Surface* gSDLScreen = nullptr;
SDL_Surface* gSDLPixels = nullptr;

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
    
    unsigned int t1 = SDL_GetTicks();

    std::thread gNewtonThread(ResetNewton);
//    ResetNewton();

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
                    break;
                case SDL_USEREVENT:
                    SDL_BlitSurface(gSDLPixels, NULL, gSDLScreen, NULL);
                    SDL_UpdateWindowSurface(gSDLWindow);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    PenDown(((SDL_MouseButtonEvent&)ev).x * kTabScale,
                            ((SDL_MouseButtonEvent&)ev).y * kTabScale);
                    break;
                case SDL_MOUSEBUTTONUP:
                    PenUp();
                    break;
                case SDL_MOUSEMOTION:
                    PenMoved(((SDL_MouseMotionEvent&)ev).x * kTabScale,
                             ((SDL_MouseMotionEvent&)ev).y * kTabScale);
                    break;
            }
        }

        unsigned int t2 = SDL_GetTicks();
        float delta = (t2 - t1) / 1000.0f;
        t1 = t2;

        // clear pixels to black background
        //SDL_FillRect(gSDLPixels, NULL, 0);

#if 0
        // write the pixels
        SDL_LockSurface(gSDLPixels);
        {
            int pitch = gSDLPixels->pitch;

            // move 100 pixels/second
            pos += delta * 100.0f;
            pos = fmodf(pos, width);

            // draw red diagonal line
            for (int i=0; i<height; i++)
            {
                int y = i;
                int x = ((int)pos + i) % width;

                unsigned int* row = (unsigned int*)((char*)gSDLPixels->pixels + pitch * y);
                row[x] = 0xff0000ff; // 0xAABBGGRR
                row[x+1] = 0xff0000ff; // 0xAABBGGRR
                row[x+2] = 0xff0000ff; // 0xAABBGGRR
                row[x+3] = 0xff0000ff; // 0xAABBGGRR
            }
        }
        SDL_UnlockSurface(gSDLPixels);

        // copy to window
        SDL_BlitSurface(gSDLPixels, NULL, gSDLScreen, NULL);
        SDL_UpdateWindowSurface(gSDLWindow);
#endif
    }

    SDL_FreeSurface(gSDLPixels);
    SDL_DestroyWindowSurface(gSDLWindow);
    SDL_DestroyWindow(gSDLWindow);
    SDL_Quit();

#if 0

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL pixels", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    SDL_Surface* screen = SDL_GetWindowSurface(window);
    SDL_Surface* pixels = SDL_CreateRGBSurfaceWithFormat(0, width, height,32, SDL_PIXELFORMAT_RGBX8888);

    // Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cout << "SDL could not be initialized!" << std::endl
        << "SDL_Error: " << SDL_GetError() << std::endl;
        return 0;
    }

#if defined linux && SDL_VERSION_ATLEAST(2, 0, 8)
    // Disable compositor bypass
    if(!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0"))
    {
        std::cout << "SDL can not disable compositor bypass!" << std::endl;
        return 0;
    }
#endif

    // Create window
    SDL_Window *window = SDL_CreateWindow("Basic C++ SDL project",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if(!window)
    {
        std::cout << "Window could not be created!" << std::endl
        << "SDL_Error: " << SDL_GetError() << std::endl;
    }
    else
    {
        // Create renderer
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if(!renderer)
        {
            std::cout << "Renderer could not be created!" << std::endl
            << "SDL_Error: " << SDL_GetError() << std::endl;
        }
        else
        {
            // Declare rect of square
            SDL_Rect squareRect;

            // Square dimensions: Half of the min(SCREEN_WIDTH, SCREEN_HEIGHT)
            squareRect.w = std::min(SCREEN_WIDTH, SCREEN_HEIGHT) / 2;
            squareRect.h = std::min(SCREEN_WIDTH, SCREEN_HEIGHT) / 2;

            // Square position: In the middle of the screen
            squareRect.x = SCREEN_WIDTH / 2 - squareRect.w / 2;
            squareRect.y = SCREEN_HEIGHT / 2 - squareRect.h / 2;


            // Event loop exit flag
            bool quit = false;

            // Event loop
            while(!quit)
            {
                SDL_Event e;

                // Wait indefinitely for the next available event
                SDL_WaitEvent(&e);

                // User requests quit
                if(e.type == SDL_QUIT)
                {
                    quit = true;
                }

                // Initialize renderer color white for the background
                SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

                // Clear screen
                SDL_RenderClear(renderer);

                // Set renderer color red to draw the square
                SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);

                // Draw filled square
                SDL_RenderFillRect(renderer, &squareRect);

                // Update screen
                SDL_RenderPresent(renderer);
            }

            // Destroy renderer
            SDL_DestroyRenderer(renderer);
        }

        // Destroy window
        SDL_DestroyWindow(window);
    }

    // Quit SDL
    SDL_Quit();
#endif
    return 0;
}
