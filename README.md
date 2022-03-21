# ExtIO_RTLSDR_LinuxPort
This repository holds a ported version of ExtIO_RTLSDR, one is compatible with linux and theoretically other OS's like Windows and MacOs

Its mostly written in C, the only reason i did not write it completely in C is because QT seems to only run with c++(although im not deep into it)

The code is a modified version of http://github.com/josemariaaraujo 's wrapper with the same intention.

On the top of my head the dependencies are:
CMake, QT 5( i think), librtlsdr(which as of writting this is hardcoded to be on /usr/lib/librtlsdr.so) to change this go on cmake, and you need some compiler
that can compile c++

This version of the readme is a really hard draft as a way for me to write somewhere, and shouldn't be taken ultra seriously

Also about licensing... yeah its complicated, i don't understand how it all works in this conoundroum as the original author wrote: "DWTFYW", the college said something about
having the IP of everything we add to the project(since i rewrote mostly everything i guess it can be a reasonable causus beli) and then theres richard's stallman LGPLv3 claim due to me using kde

heres the document for richard stallman one
https://www.gnu.org/licenses/lgpl-3.0.en.html

a republics article(basically a "law notice") about the licensing rights refered by the college
https://ead.ipleiria.pt/2021-22/pluginfile.php/155131/mod_resource/content/2/Regulamento_de_propriedade_intelectual_do_IPL.pdf

Also i don't feel like doing the url's right now or other markdown formatting if you're seeing this message, its a TODO here
