/*
 * Acid Cam v2 - OpenCV Edition
 * written by Jared Bruni ( http://lostsidedead.com / https://github.com/lostjared )
 
 GitHub: http://github.com/lostjared
 Website: http://lostsidedead.com
 YouTube: http://youtube.com/LostSideDead
 Instagram: http://instagram.com/jaredbruni
 Twitter: http://twitter.com/jaredbruni
 Facebook: http://facebook.com/LostSideDead0x
 
 You can use this program free of charge and redistrubute as long
 as you do not charge anything for this program. This program is 100%
 Free.
 
 BSD 2-Clause License
 
 Copyright (c) 2017, Jared Bruni
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 */
#include"acidcam-cli.hpp"
#include<sys/stat.h>
#include<sys/types.h>
#include<signal.h>

extern void control_Handler(int sig);

namespace cmd {
    
    void setCursorPos(int y, int x) {
        std::cout << "\033[" << (y+1) << ";" << (x+1) << "H";
    }
    
    void clearCursor() {
        std::cout << "\033[H\033[J";
    }
    
    std::ostream &operator<<(std::ostream &out, const File_Type &type) {
        if(type == File_Type::MOV)
            out << "MPEG-4 (Quicktime)";
        else
            out << "XviD";
        return out;
    }

    AC_Program::AC_Program() {
        library = nullptr;
    }
    
    AC_Program::~AC_Program() {
        if(library != nullptr)
            dlclose(library);
    }
    
    bool AC_Program::loadPlugin(const std::string &s) {
        library = dlopen(s.c_str(), RTLD_LAZY);
        if(library == NULL)
            return false;
        
        void *addr = dlsym(library, "filter");
        const char *err = dlerror();
        if(err) {
            std::cerr << "Could not locate function: filter in " << s << "\n";
            return false;
        }
        plugin = reinterpret_cast<plugin_filter>(addr);
        return true;
    }
    
    void AC_Program::callPlugin(cv::Mat &frame) {
        if(library != nullptr)
        	plugin(frame);
    }
    
    bool AC_Program::initProgram(const File_Type &ftype, bool visible, const std::string &input, const std::string &output, std::vector<unsigned int> &filter_list,std::vector<unsigned int> &col) {
        file_type = ftype;
        is_visible = visible;
        input_file = input;
        output_file = output;
        filters = filter_list;
        capture.open(input_file);
        if(!capture.isOpened()) {
            std::cerr << "acidcam: Error could not open file: " << input_file << "\n";
            return false;
        }
        int aw = static_cast<int>(capture.get(CV_CAP_PROP_FRAME_WIDTH));
        int ah = static_cast<int>(capture.get(CV_CAP_PROP_FRAME_HEIGHT));
        double fps = capture.get(CV_CAP_PROP_FPS);
        
        if(fps < 1) {
            std::cerr << "acidcam: Invalid frame rate...\n";
            exit(EXIT_FAILURE);
        }
        
        if(file_type == File_Type::MOV)
            writer.open(output_file, CV_FOURCC('m', 'p', '4', 'v'), fps, cv::Size(aw, ah), true);
        else
            writer.open(output_file, CV_FOURCC('X', 'V', 'I', 'D'), fps, cv::Size(aw, ah), true);
        if(!writer.isOpened()) {
            std::cerr << "acidcam: Error could not open file for writing: " << output_file << "\n";
            return false;
        }
        unsigned int num_frames = capture.get(CV_CAP_PROP_FRAME_COUNT);
        std::cout << "acidcam: input[" << input_file << "] output[" << output_file << "] width[" << aw << "] height[" << ah << "] fps[" << fps << "] length[" << static_cast<unsigned int>((num_frames/fps)) << " seconds] "<< "format[" << file_type << "]\n";
        std::cout << "\nFilters to Apply: \n";
        for(unsigned int q = 0; q < filter_list.size(); ++q) {
            std::cout << ac::draw_strings[filter_list[q]] << "\n";
        }
        std::cout << "\n";
        if(col.size()==3) {
            ac::swapColor_b = static_cast<unsigned char>(col[0]);
            ac::swapColor_g = static_cast<unsigned char>(col[1]);
            ac::swapColor_r = static_cast<unsigned char>(col[2]);
            std::cout << "Add RGB {" << col[0] << ", " << col[1] << ", " << col[2] << "}\n";
        }
        return true;
    }
    
    void AC_Program::stop() {
        active = false;
        setCursorPos(filters.size()+3, 0);
    }
    
    void AC_Program::run() {
        unsigned long frame_count_len = 0, frame_index = 0;
        unsigned int percent_now = 0;
        try {
            bool copy_orig = false;
            if(std::find(filters.begin(), filters.end(), ac::filter_map["Blend with Source"]) != filters.end()) {
                copy_orig = true;
            }
            frame_count_len = capture.get(CV_CAP_PROP_FRAME_COUNT);
            struct sigaction sa;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sa.sa_handler = control_Handler;
            if(sigaction(SIGINT, &sa, 0) == -1) {
                std::cerr << "Error on sigaction:\n";
                exit(EXIT_FAILURE);
            }
            if(is_visible)
                cv::namedWindow("acidcam_cli");
            active = true;
            
            setCursorPos(5+filters.size(), 0);
            std::cout << "acidcam: Working frame: [0/" << frame_count_len << "] - 0% Size: 0 MB \n";
            while(active == true) {
                cv::Mat frame;
                if(capture.read(frame) == false) {
                    break;
                }
                if(copy_orig == true) ac::orig_frame = frame.clone();
                frame_index ++;
                if(frame_index >= frame_count_len) {
                    break;
                }
                for(unsigned int i = 0; i < filters.size(); ++i) {
                    ac::draw_func[filters[i]](frame);
                }
                writer.write(frame);
                double val = frame_index;
                double size = frame_count_len;
                if(size != 0) {
                    double percent = (val/size)*100;
                    unsigned int percent_trunc = static_cast<unsigned int>(percent);
                    if(percent_trunc > percent_now) {
                        percent_now = percent_trunc;
                        struct stat buf;
                        lstat(output_file.c_str(), &buf);
                        setCursorPos(5+filters.size(), 0);
                        std::cout << "acidcam: Working frame: [" << frame_index << "/" << frame_count_len << "] - " << percent_trunc << "% Size: " << ((buf.st_size/1024)/1024) << " MB\n";
                    }
                }
                if(is_visible) {
                    cv::imshow("acidcam_cli", frame);
                    int key = cv::waitKey(25);
                    if(key == 27) break;
                }
            }
        } catch(...) {
            writer.release();
            std::cerr << "acidcam: Error exception occoured..\n";
        }
        if(percent_now == 99) percent_now = 100;
        setCursorPos(5+filters.size(), 0);
        std::cout << "acidcam: " << percent_now << "% Done wrote to file [" << output_file << "] format[" << file_type << "]\n";
    }
}
