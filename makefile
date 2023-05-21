files := toojpeg.cpp parser.cpp

parser: $(files)
	g++ -o parser $(files)