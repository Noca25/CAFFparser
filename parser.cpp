#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <stdio.h>

#include "toojpeg.h"

std::vector<unsigned char> globalResultPixels;

namespace fileparser {

    struct CiffHeader {
        char magic[4];
        uint64_t header_size;
        uint64_t content_size;
        uint64_t width;
        uint64_t height;
        std::string caption;
        std::vector<std::string> tags;
    };

    struct Pixel {
        unsigned char red;
        unsigned char green;
        unsigned char blue; 
    };

    struct CiffFile {
        CiffHeader header;
        std::vector<Pixel> pixels;
    };

    struct CaffHeader {
        char magic[4];
        uint64_t header_size;
        uint64_t num_anim;
    };

    struct CaffCredits {
        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint64_t creator_len;
        std::string creator;
    };

    struct CaffAnimation {
        uint64_t duration;
        CiffFile ciff;
    };


    class CiffParser {
        public:
        int parse(const std::string& pathToRead, const std::string& fileName) {
            std::ifstream inputStream( pathToRead, std::ios::binary );
            return parse (inputStream, fileName);
        }

        int parse (std::ifstream& inputStream, const std::string& fileName) {
            
            // Read HEADER
            auto header = CiffHeader();
            inputStream.read(reinterpret_cast<char*>(&header.magic), 4); // https://en.cppreference.com/w/cpp/io/basic_istream/read
            std::cout << "Magic: " << header.magic << std::endl; 
            if(header.magic != std::string("CIFF")){
                std::cerr << "Error: CIFF file was read, but magic was not CIFF" << std::endl;
                return -1;
            }            
            inputStream.read(reinterpret_cast<char*>(&header.header_size), 8);
            std::cout << "Header size: " << header.header_size << std::endl; 
            inputStream.read(reinterpret_cast<char*>(&header.content_size), 8);
            std::cout << "Content size: " << header.content_size << std::endl;
            inputStream.read(reinterpret_cast<char*>(&header.width), 8);
            std::cout << "Width: " << header.width << std::endl; 
            inputStream.read(reinterpret_cast<char*>(&header.height), 8);
            std::cout << "Height: " << header.height << std::endl; 
            
            if(header.width * header.height * 3 != header.content_size) {
                std::cerr << "Error: CIFF file width (" << header.width << "), height (" << header.height << "), and content size("<< header.content_size <<") doesn't match" << std::endl;
                return -1;
            }
                        
            char characterBuffer;
            bool stop = false;
            while(!stop) {
                inputStream.read(reinterpret_cast<char*>(&characterBuffer), 1); 
                if(characterBuffer != '\n') {
                    header.caption += characterBuffer;
                } else {
                    stop = true;
                }
            }

            auto used_size =  
                sizeof(header.magic) +
                sizeof(header.header_size) + 
                sizeof(header.content_size) + 
                sizeof(header.width) + 
                sizeof(header.height) +
                header.caption.size() +
                1 // \n character at the end of caption
            ;

            if(used_size > header.header_size) {
                std::cerr << "Error: Already more characters used than the header size." << std::endl; 
                return -1;
            }

            uint remaining_size = header.header_size - used_size;

            stop = false;
            std::string temp = "";
            int tag_characters_read = 0;
            while(!stop) {
                inputStream.read(reinterpret_cast<char*>(&characterBuffer), 1); 
                tag_characters_read++;
                if(characterBuffer == '\0'){
                    header.tags.push_back(temp);
                    temp = "";
                    if(tag_characters_read == remaining_size){
                        stop = true;
                    }
                } else {
                    temp += characterBuffer;
                    if(tag_characters_read == remaining_size){
                        std::cerr << "Error: Ran out of expected characters, yet the header didn't end with a \\0" << std::endl;
                        return -1;
                    }
                }
            }

            char pixelBuffer[header.content_size];
            inputStream.read(reinterpret_cast<char*>(&pixelBuffer), header.content_size); 
            uint amountOfPixels = header.content_size / 3;
            Pixel resultPixels [amountOfPixels];

            for (int i = 0; i < amountOfPixels; i++) {
                resultPixels[i].red = pixelBuffer[3*i];
                resultPixels[i].green = pixelBuffer[3*i+1];
                resultPixels[i].blue =pixelBuffer[3*i+2];
            }

            std::cout << "CIFF file processed successfully" << std::endl;

            //Converting to JPEG

            auto myOutput = [](unsigned char oneByte) { 
                globalResultPixels.push_back(oneByte); 
            };

            TooJpeg::writeJpeg(myOutput, pixelBuffer, header.width, header.height);

            std::ofstream fout;

            fout.open(fileName + ".jpg", std::ios::binary | std::ios::out);

            for(auto i : globalResultPixels) {
                fout.write((char *)&i, 1); 
            }

            fout.close();

            return 0;
        }

    };

    class CaffParser {
        public:

        int parse(const std::string& pathToRead, const std::string& fileName){
            // Read File
            std::ifstream inputStream( pathToRead, std::ios::binary );
            
            bool stop = false;
            
            while(!stop) {
             
                uint8_t currentBlockID;
                inputStream.read(reinterpret_cast<char*>(&currentBlockID), 1);
                std::cout << "Current block: " << +currentBlockID << std::endl; 
                uint64_t lengthOfCurrentBlock;
                inputStream.read(reinterpret_cast<char*>(&lengthOfCurrentBlock), 8);
                std::cout << "Current block length: " << lengthOfCurrentBlock << std::endl;
            
                if(currentBlockID == 1) {    
                    CaffHeader caffHeader;
                    if(lengthOfCurrentBlock != 20){
                        std::cerr << "Error: Header blocksize should be 20" << std::endl;
                        return -1;
                    }
                    inputStream.read(reinterpret_cast<char*>(&caffHeader.magic), 4);
                    std::string caffHeaderMagicString = 
                        std::string("") + 
                        caffHeader.magic[0] + 
                        caffHeader.magic[1] + 
                        caffHeader.magic[2] + 
                        caffHeader.magic[3];
                    std::cout << "CaffHeader Magic: " << caffHeaderMagicString << std::endl;
                    if(caffHeaderMagicString != std::string("CAFF")) {
                        std::cerr << "Error: Caffheader magic mismatch" << std::endl;
                        return -1;
                    }
                    inputStream.read(reinterpret_cast<char*>(&caffHeader.header_size), 8);
                    std::cout << "CaffHeader size: " << caffHeader.header_size << std::endl;
                    inputStream.read(reinterpret_cast<char*>(&caffHeader.num_anim), 8);
                    std::cout << "Number of animations: " << caffHeader.num_anim << std::endl;                  
                    if(caffHeader.num_anim < 1) {
                        std::cerr << "Error: There are no animations :(" << std::endl;
                    }
                }
                else if (currentBlockID == 2)  {
                    CaffCredits credits;
                    inputStream.read(reinterpret_cast<char*>(&credits.year), 2);
                    std::cout << "Year: " << +credits.year  << std::endl;
                    inputStream.read(reinterpret_cast<char*>(&credits.month), 1);
                    std::cout << "Month: " << +credits.month  << std::endl;
                    inputStream.read(reinterpret_cast<char*>(&credits.day), 1);
                    std::cout << "Day: " << +credits.day  << std::endl;
                    inputStream.read(reinterpret_cast<char*>(&credits.hour), 1);
                    std::cout << "Hour: " << +credits.hour  << std::endl;
                    inputStream.read(reinterpret_cast<char*>(&credits.minute), 1);
                    std::cout << "Minute: " << +credits.minute  << std::endl;
                    inputStream.read(reinterpret_cast<char*>(&credits.creator_len), 8);
                    std::cout << "Credits creator length: " << credits.creator_len << std::endl;
                    auto credits_used_size =  
                        sizeof(credits.year) +
                        sizeof(credits.month) + 
                        sizeof(credits.day) + 
                        sizeof(credits.hour) + 
                        sizeof(credits.minute) +
                        sizeof(credits.creator_len) +
                        credits.creator_len
                    ;
                    
                    if(credits.creator_len > lengthOfCurrentBlock) {
                        std::cerr << "Error: Length of creator is bigger than block size." << std::endl; 
                        return -1;
                    }
                    if(credits_used_size != lengthOfCurrentBlock) {
                        std::cerr << "Error: Blocksize mismatch, the length of the current block is " <<
                            lengthOfCurrentBlock << " but the credits block size will add up to " << credits_used_size << std::endl;
                    }

                    char authorNameBuffer [credits.creator_len + 1]; // +1 for the \0                    
                    authorNameBuffer [credits.creator_len] = '\0';                    
                    inputStream.read(reinterpret_cast<char*>(&authorNameBuffer), credits.creator_len);
                    credits.creator = authorNameBuffer;
                    std::cout << "Author: " << credits.creator << std::endl;

                }  
                else if(currentBlockID == 3) {
                    CaffAnimation animation;
                    CiffParser ciffParser;
                    inputStream.read(reinterpret_cast<char*>(&animation.duration), 8);
                    return ciffParser.parse(inputStream, fileName);
                    stop = true;
                }
                else {
                    std::cerr << "Error: Invalid block ID" << std::endl;
                    return -1;
                }    
            }
            return 0;
        }
    };

}

std::string getFileNameWithoutExtension(std::string path) {
    size_t lastindex = path.find_last_of("."); 
    return path.substr(0, lastindex); 
}

int main(int argc, char* argv[]) { 
    if(argc != 3) {
        std::cerr << "Error: number of arguments is incorrect" << std::endl;
        return -1;
    } if (!std::filesystem::exists(argv[2])) { 
        std::cerr << "Error: file doesn't exist" << std::endl;
        return -1;
    } else {
        auto fileName = getFileNameWithoutExtension(argv[2]); // std::filesystem::path((argv[2])).stem();
        if( argv[1] == std::string("-ciff")) {
            std::cout << "Preparing to read ciff file" << std::endl;
            auto ciffParser = fileparser::CiffParser();
            return ciffParser.parse(argv[2], fileName);
            
        } else if ( argv[1] == std::string("-caff")) {
            std::cout << "Preparing to read caff file" <<std::endl;
            auto caffParser = fileparser::CaffParser();
            return caffParser.parse(argv[2], fileName);

        } else {
            std::cerr << "Error: first parameter is incorrect" << std::endl;
            return -1;
        }
    }
}
