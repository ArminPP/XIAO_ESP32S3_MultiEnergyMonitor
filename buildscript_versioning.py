import datetime
tm = datetime.datetime.today()

FILENAME_BUILDNO = 'versioning'
FILENAME_VERSION_H = 'include/Version.h'
version = 'v0.5.' + str(tm.year)[-2:]+('0'+str(tm.month))[-2:]+('0'+str(tm.day))[-2:] + '_'

build_no = 0
try:
    with open(FILENAME_BUILDNO) as f:
        build_no = int(f.readline()) + 1
except:
    print('Starting build number from 1..')
    build_no = 1
with open(FILENAME_BUILDNO, 'w+') as f:
    f.write(str(build_no))
    print('Build number: {}'.format(build_no))

hf = """
#define Version "USS V0" // Armins Version: is static for the project! 
#ifndef BUILD_NUMBER
  #define BUILD_NUMBER "{}"
#endif
#ifndef VERSION
  #define VERSION "{} - {}"
#endif
#ifndef VERSION_SHORT
  #define VERSION_SHORT "{}"
#endif
""".format(build_no, version+str(build_no), datetime.datetime.now(), version+str(build_no))
with open(FILENAME_VERSION_H, 'w+') as f:
    f.write(hf)
