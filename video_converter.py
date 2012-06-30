############################################################
# video converter software to convert any video to         #
# .vin format, (special format for the avr video player    #
# author: Vinod S                                          #
#         vinodstanur at gmail dot com                     #
#home:  http://blog.vinu.co.in                             #
#USAGE:                                                    #
# python converter.py input_file_name seek_time duration   #
# example: python converter.py a.avi 0:05:00 00:01:00      #
# above code will convert a.avi from time = 5 minutes to   # 
#  6 minutes                                               #
############################################################
 
#!/usr/bin/python
import time,sys,os,Image
time = sys.argv[3]
seek = sys.argv[2]
videofile = sys.argv[1]
videofile = videofile.replace(" ","\ ")
videofile = videofile.replace("(","\(")
videofile = videofile.replace(")","\)")
os.system("clear")
count = 1
os.system("rm clip.avi")
os.system("rm *.jpeg audio.wav")
os.system("ffmpeg -ss " + seek + " -t " + time + " -y -i " + videofile + " -acodec libmp3lame -vcodec msmpeg4 -ab 8k -b 1000k -vf scale=132:65 -ar 8000 -r:v 9 clip.avi")
os.system("ffmpeg -i clip.avi %d.jpeg")
os.system("ffmpeg -i "+ videofile + " -ac 1 -acodec pcm_u8 -ac 1 -ar 11000 -t "+ time + " -ss "+ seek  + " -vn audio.wav")
files = os.listdir(".")
afd = open("audio.wav")
print afd.read(44)
a=0
b=0
total_jpeg = 0
for i in files:
    if i[-4:]=="jpeg":
        total_jpeg =total_jpeg+ 1
total_jpeg = total_jpeg-1
print str(total_jpeg)
f=open("out.vin","w")
acount=0
skip = 0
while count < total_jpeg:
    print str(count)
    v = Image.open(str(count)+".jpeg").convert("RGB")
    for i in range(65):
        for j in range(132):
            if acount==7:
                if skip == 25*14:
                    skip = 0
                    f.write(readdata)
                else:
                    readdata=afd.read(1)
                    f.write(readdata)
                    skip = skip + 1
                acount = 0
            acount = acount + 1
            tup = v.getpixel((131-j,i))
            a=(tup[2]&248)|(tup[1]>>5)
            b=((tup[1]<<3)&224)|(tup[0]>>3)
            f.write(chr(a))
            f.write(chr(b))
    count = count + 1
    del v
f.close()
os.system("rm clip.avi")
os.system("rm *.jpeg")
os.system("rm audio.wav")
