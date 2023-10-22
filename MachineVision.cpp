// This file is part of the Orbbec Astra SDK [https://orbbec3d.com]
// Copyright (c) 2015-2017 Orbbec 3D
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Be excellent to each other.
#include <SFML/Graphics.hpp>
#include <astra/astra.hpp>
#include "LitDepthVisualizer.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <key_handler.h>
#include <sstream>




bool flag = false;

class DepthFrameListener : public astra::FrameListener
{
public:
    DepthFrameListener()
    {
        prev_ = ClockType::now();
        font_.loadFromFile("Inconsolata.otf");
    }

    void init_texture(int width, int height)
    {
        if (!displayBuffer_ ||
            width != displayWidth_ ||
            height != displayHeight_)
        {
            displayWidth_ = width;
            displayHeight_ = height;

            // texture is RGBA
            const int byteLength = displayWidth_ * displayHeight_ * 4;

            displayBuffer_ = BufferPtr(new uint8_t[byteLength]);
            std::fill(&displayBuffer_[0], &displayBuffer_[0] + byteLength, 0);

            texture_.create(displayWidth_, displayHeight_);
            sprite_.setTexture(texture_, true);
            sprite_.setPosition(0, 0);
        }
    }

    void check_fps()
    {
        const float frameWeight = .2f;

        const ClockType::time_point now = ClockType::now();
        const float elapsedMillis = std::chrono::duration_cast<DurationType>(now - prev_).count();

        elapsedMillis_ = elapsedMillis * frameWeight + elapsedMillis_ * (1.f - frameWeight);
        prev_ = now;

        const float fps = 1000.f / elapsedMillis;

        const auto precision = std::cout.precision();

        /*std::cout << std::fixed
                  << std::setprecision(1)
                  << fps << "fps ("
                  << std::setprecision(1)
                  << elapsedMillis_ << " ms)"
                  << std::setprecision(precision)
                  << std::endl;
         */

        /*****************************

        

        if (mouseWorldZ_ > 1200 && mouseWorldZ_ < 1250 && mouseY_ < 275 && mouseY_ > 220)
        {
            std::cout << std::fixed
                        << std::setprecision(0)
                        << "(" << mouseX_ << ", " << mouseY_ << ") "
                        << "X: " << mouseWorldX_ << " Y: " << mouseWorldY_ << " Z: " << mouseWorldZ_
                        << std::endl << std::endl;
        }
        
        *****************************/


    }

    float getMouseZ() 
    {
        return mouseWorldZ_;
    }

    /*----------------------------------------*/

    void screenshot_window(sf::RenderWindow& render_window, std::string filename)
    {
        sf::Texture texture;
        texture.create(render_window.getSize().x, render_window.getSize().y);
        texture.update(render_window);
        if (texture.copyToImage().saveToFile(filename))
        {
            std::cout << "Screenshot saved to " << filename << std::endl;
        }
    }

    /*----------------------------------------*/

    void on_frame_ready(astra::StreamReader& reader,
                        astra::Frame& frame) override
    {
        const astra::PointFrame pointFrame = frame.get<astra::PointFrame>();
        const int width = pointFrame.width();
        const int height = pointFrame.height();

        init_texture(width, height);

        check_fps();

        

        

        if (isPaused_) { return; }

        copy_depth_data(frame);

        visualizer_.update(pointFrame);

        const astra::RgbPixel* vizBuffer = visualizer_.get_output();
        for (int i = 0; i < width * height; i++)
        {
            const int rgbaOffset = i * 4;
            displayBuffer_[rgbaOffset] = vizBuffer[i].r;
            displayBuffer_[rgbaOffset + 1] = vizBuffer[i].b;
            displayBuffer_[rgbaOffset + 2] = vizBuffer[i].g;
            displayBuffer_[rgbaOffset + 3] = 255;
        }

        texture_.update(displayBuffer_.get());
    }

    void copy_depth_data(astra::Frame& frame)
    {
        const astra::DepthFrame depthFrame = frame.get<astra::DepthFrame>();

        if (depthFrame.is_valid())
        {
            const int width = depthFrame.width();
            const int height = depthFrame.height();
            if (!depthData_ || width != depthWidth_ || height != depthHeight_)
            {
                depthWidth_ = width;
                depthHeight_ = height;

                // texture is RGBA
                const int byteLength = depthWidth_ * depthHeight_ * sizeof(uint16_t);

                depthData_ = DepthPtr(new int16_t[byteLength]);
            }

            depthFrame.copy_to(&depthData_[0]);
        }
    }

    void update_mouse_position(sf::RenderWindow& window,
                               const astra::CoordinateMapper& coordinateMapper)
    {
        const sf::Vector2i position = sf::Mouse::getPosition(window);
        const sf::Vector2u windowSize = window.getSize();

        float mouseNormX = position.x / float(windowSize.x);
        float mouseNormY = position.y / float(windowSize.y);

        mouseX_ = depthWidth_ * mouseNormX;
        mouseY_ = depthHeight_ * mouseNormY;

        if (mouseX_ >= depthWidth_ ||
            mouseY_ >= depthHeight_ ||
            mouseX_ < 0 ||
            mouseY_ < 0) { return; }

        const size_t index = (depthWidth_ * mouseY_ + mouseX_);
        const short z = depthData_[index];

        coordinateMapper.convert_depth_to_world(float(mouseX_),
                                                float(mouseY_),
                                                float(z),
                                                mouseWorldX_,
                                                mouseWorldY_,
                                                mouseWorldZ_);
    }

    void draw_text(sf::RenderWindow& window,
                   sf::Text& text,
                   sf::Color color,
                   const int x,
                   const int y) const
    {
        text.setColor(sf::Color::Black);
        text.setPosition(x + 5, y + 5);
        window.draw(text);

        text.setColor(color);
        text.setPosition(x, y);
        window.draw(text);

        




     
    }

    void draw_mouse_overlay(sf::RenderWindow& window,
                            const float depthWScale,
                            const float depthHScale) const
    {
        if (!isMouseOverlayEnabled_ || !depthData_) { return; }

        std::stringstream str;
        str << std::fixed
            << std::setprecision(0)
            << "(" << mouseX_ << ", " << mouseY_ << ") "
            << "X: " << mouseWorldX_ << " Y: " << mouseWorldY_ << " Z: " << mouseWorldZ_;

        const int characterSize = 40;
        sf::Text text(str.str(), font_);
        text.setCharacterSize(characterSize);
        text.setStyle(sf::Text::Bold);

        const float displayX = 10.f;
        const float margin = 10.f;
        const float displayY = window.getView().getSize().y - (margin + characterSize);

        draw_text(window, text, sf::Color::White, displayX, displayY);
    }

    void draw_to(sf::RenderWindow& window)
    {
        if (displayBuffer_ != nullptr)
        {
            const float depthWScale = window.getView().getSize().x / displayWidth_;
            const float depthHScale = window.getView().getSize().y / displayHeight_;

            sprite_.setScale(depthWScale, depthHScale);
            window.draw(sprite_);

            draw_mouse_overlay(window, depthWScale, depthHScale);

            float minx = minX();
            float maxx = maxX();
            float miny = minY(minx, maxx);
            float maxy = maxY(minx, maxx);

            

            sf::RectangleShape rectangle;
            rectangle.setSize(sf::Vector2f(2*(maxx-minx), 2*(maxy-miny)));
            rectangle.setOutlineColor(sf::Color::Red);
            rectangle.setOutlineThickness(3);
            rectangle.setFillColor(sf::Color(0, 0, 0, 0));
            rectangle.setPosition(2*minx, 2*miny);

            window.draw(rectangle);



            sf::CircleShape circle;
            float radius = 15;
            circle.setRadius(radius);
            circle.setOutlineColor(sf::Color::Green);
            circle.setOutlineThickness(2);
            circle.setFillColor(sf::Color(0, 0, 0, 0));
            circle.setPosition((2 * maxx)-50, (2 * maxy)-20);

            window.draw(circle);

            float maxPll = maxPole();
            float minPll = minPole();

            float cPos = ((maxx-25));
            /*
            coordinateMapper.convert_depth_to_world(float(mouseX_),
                float(mouseY_),
                float(z),
                mouseWorldX_,
                mouseWorldY_,
                mouseWorldZ_);
            */
            float pollAvg = (maxPll + minPll) / 2.0;
            //std::cout << (int)cPos << std::endl << (int) pollAvg << std::endl << std::endl;
            if (((int)cPos > (int)minPll) && ((int)cPos < (int)maxPll) && (flag == false))
            {
                screenshot_window(window, "picture.png");
                flag = true;



            }

            //Convert Radial Distance to real-world position

            float zeroX = minx - 5;
            float distToStart = ((0.214 * zeroX - 7.33)) * 10;
            if (distToStart > 0)
                std::cout << std::setprecision(4) << " Distance from start of conveyor: " << distToStart << " mm" << std::endl;



        }
    }

    

    void toggle_paused()
    {
        isPaused_ = !isPaused_;
    }

    bool is_paused() const
    {
        return isPaused_;
    }

    void toggle_overlay()
    {
        isMouseOverlayEnabled_ = !isMouseOverlayEnabled_;
    }

    bool overlay_enabled() const
    {
        return isMouseOverlayEnabled_;
    }


    float minX() 
    {
        float min = 100000;
        float x = 0;
        for (int index = 137700; index < 163840; index++)
        {
            if (depthData_[index] > 1180 && depthData_[index] < 1300)
            {  
                x = index%640;
                if (x < min)
                    min = x;
            }   
        }
        return min;
    }

    float maxX() 
    {
        float max = 0;
        float x = 0;
        for (int index = 137700; index < 163840; index++)
        {
            if (depthData_[index] > 1180 && depthData_[index] < 1300)
            {
                x = index%640;
                if (x > max)
                    max = x;
            }  
        }
        return max;
    }

    float minY(float minX, float maxX) 
    {
        float min = 100000;
        float y = 0;
        for (int index = 137700; index < 163840; index++)
        {
            if (depthData_[index] > 1180 && depthData_[index] < 1300)
            {
                for (int i = minX; i <= maxX; i++)
                {
                    y = (index - i) / 640;
                    if (y < min)
                        min = y;
                }  
            }
        }
        return min;
    }

    float maxY(float minX, float maxX) 
    {
        float max = 0;
        float y = 0;
        for (int index = 137700; index < 163840; index++)
        {
            if (depthData_[index] > 1180 && depthData_[index] < 1300)
            {
                for (int i = minX; i <= maxX; i++)
                {
                    float y = (index - i) / 640;
                    if (y > max)
                        max = y;
                }
            }
        }
        return max;
    }
    
    float minPole()
    {
        float min = 100000;
        float x = 0;
        for (int index = 87240; index < 102800; index++)
        {
            if (depthData_[index] > 1310 && depthData_[index] < 1350)
            {
                x = index % 640;
                if (x < min)
                    min = x;
            }
        }
        return min;
    }
    
    float maxPole()
    {
        float max = 0;
        float x = 0;
        for (int index = 87240; index < 102800; index++)
        {
            if (depthData_[index] > 1310 && depthData_[index] < 1350)
            {
                x = index % 640;
                if (x > max)
                    max = x;
            }
        }
        return max;
    }
  

private:
    samples::common::LitDepthVisualizer visualizer_;

    using DurationType = std::chrono::milliseconds;
    using ClockType = std::chrono::high_resolution_clock;

    ClockType::time_point prev_;
    float elapsedMillis_{.0f};

    sf::Texture texture_;
    sf::Sprite sprite_;
    sf::Font font_;

    int displayWidth_{0};
    int displayHeight_{0};

    using BufferPtr = std::unique_ptr<uint8_t[]>;
    BufferPtr displayBuffer_{nullptr};

    int depthWidth_{0};
    int depthHeight_{0};

    using DepthPtr = std::unique_ptr<int16_t[]>;
    DepthPtr depthData_{nullptr};

    int mouseX_{0};
    int mouseY_{0};
    float mouseWorldX_{0};
    float mouseWorldY_{0};
    float mouseWorldZ_{0};
    bool isPaused_{false};
    bool isMouseOverlayEnabled_{true};
};

astra::DepthStream configure_depth(astra::StreamReader& reader)
{
    auto depthStream = reader.stream<astra::DepthStream>();

    auto oldMode = depthStream.mode();

    //We don't have to set the mode to start the stream, but if you want to here is how:
    astra::ImageStreamMode depthMode;

    depthMode.set_width(640);
    depthMode.set_height(480);
    depthMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_DEPTH_MM);
    depthMode.set_fps(30);

    depthStream.set_mode(depthMode);

    auto newMode = depthStream.mode();
    printf("Changed depth mode: %dx%d @ %d -> %dx%d @ %d\n",
        oldMode.width(), oldMode.height(), oldMode.fps(),
        newMode.width(), newMode.height(), newMode.fps());

    return depthStream;
}



int main(int argc, char** argv)
{
    astra::initialize();

    set_key_handler();

    sf::RenderWindow window(sf::VideoMode(1280, 960), "Depth Viewer");

#ifdef _WIN32
    auto fullscreenStyle = sf::Style::None;
#else
    auto fullscreenStyle = sf::Style::Fullscreen;
#endif

    const sf::VideoMode fullScreenMode = sf::VideoMode::getFullscreenModes()[0];
    const sf::VideoMode windowedMode(1280, 1024);
    bool isFullScreen = false;

    astra::StreamSet streamSet;
    astra::StreamReader reader = streamSet.create_reader();
    reader.stream<astra::PointStream>().start();

    auto depthStream = configure_depth(reader);
    depthStream.start();

    DepthFrameListener listener;

    reader.add_listener(listener);
    

    while (window.isOpen())
    {
        /*
        float minx = listener.minX();
        float maxx = listener.maxX();
        float miny = listener.minY(minx, maxx);
        float maxy = listener.maxY(minx, maxx);
        */

        astra_update();

        

        sf::Event event;
        while (window.pollEvent(event))
        {
            

            switch (event.type)
            {
            case sf::Event::Closed:
                window.close();
                break;
            case sf::Event::KeyPressed:
            {
                if (event.key.code == sf::Keyboard::C && event.key.control)
                {
                    window.close();
                }
                switch (event.key.code)
                {
                case sf::Keyboard::D:
                {
                    auto oldMode = depthStream.mode();
                    astra::ImageStreamMode depthMode;

                    depthMode.set_width(640);
                    depthMode.set_height(400);
                    depthMode.set_pixel_format(astra_pixel_formats::ASTRA_PIXEL_FORMAT_DEPTH_MM);
                    depthMode.set_fps(30);

                    depthStream.set_mode(depthMode);
                    auto newMode = depthStream.mode();
                    printf("Changed depth mode: %dx%d @ %d -> %dx%d @ %d\n",
                           oldMode.width(), oldMode.height(), oldMode.fps(),
                           newMode.width(), newMode.height(), newMode.fps());
                    break;
                }
                case sf::Keyboard::B:
                {

                    float minx = listener.minX();
                    float maxx = listener.maxX();
                    float miny = listener.minY(minx, maxx);
                    float maxy = listener.maxY(minx, maxx);
                    std::cout << std::endl << std::endl << std::endl << minx << std::endl << maxx << std::endl << miny << std::endl << maxy << std::endl << std::endl;
                    

                    break;
                }
                case sf::Keyboard::Escape:
                    window.close();
                    break;
                case sf::Keyboard::F:
                    if (isFullScreen)
                    {
                        window.create(windowedMode, "Depth Viewer", sf::Style::Default);
                    }
                    else
                    {
                        window.create(fullScreenMode, "Depth Viewer", fullscreenStyle);
                    }
                    isFullScreen = !isFullScreen;
                    break;
                case sf::Keyboard::R: // hold exposure
                    depthStream.enable_registration(!depthStream.registration_enabled());
                    break;
                case sf::Keyboard::M: // mirror
                    depthStream.enable_mirroring(!depthStream.mirroring_enabled());
                    break;
                case sf::Keyboard::P: // pause
                    listener.toggle_paused();
                    /**/
                    listener.screenshot_window(window, "testfile.png");
                    /**/
                    break;
                case sf::Keyboard::Space: // hides and toggles the numbers at the bottom of the screen
                    listener.toggle_overlay();
                    break;

                default:
                    break;
                }
                break;
            }
            case sf::Event::MouseMoved:
            {
                auto coordinateMapper = depthStream.coordinateMapper();
                listener.update_mouse_position(window, coordinateMapper);
                break;
            }
            default:
                break;
            }
        }

        // clear the window with black color
        window.clear(sf::Color::Black);

        listener.draw_to(window);
        window.display();

        if (!shouldContinue)
        {
            window.close();
        }
    }

    astra::terminate();

    return 0;
}



