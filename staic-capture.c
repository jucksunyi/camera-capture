/*
 * * V4L2 video capture example
 * *
 * * This program can be used and distributed without restrictions.
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h> /* for videodev2.h */
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <linux/kd.h>
//#include <linux/input.h>
//#include <linux/input-event-codes.h> 

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define RGB(b,g,r) (unsigned char[4]){(b),(g),(r),0}

typedef enum {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
} io_method;

struct buffer {
    void * start;
    size_t length;
};
static char * dev_name = NULL;
static int bp=-1;
//  static int key=-1;
static int tty=-1;
struct fb_var_screeninfo  vinfo;
struct fb_fix_screeninfo  finfo;
//        struct input_event t;
long screensize=0;
char *fbp = 0;
long location = 0;

static io_method io = IO_METHOD_MMAP;
static int fd = -1;
struct buffer * buffers = NULL;

static unsigned int n_buffers = 0;
static FILE *fp=NULL;
static void
errno_exit (const char * s)
{
    fprintf (stderr, "%s error %d, %s\n",s, errno, strerror (errno));
    exit (EXIT_FAILURE);

}
static int xioctl (int fd,int request,void * arg)
{
    int r;
    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    return r;
}

void fb_putpixel(int x,int y,unsigned char pixel[4]){
    unsigned long position=0;
    position=((unsigned long)x+((unsigned long)y)*(vinfo.xres))*4ul;
    lseek(bp,position,SEEK_SET);
    write(bp,pixel,4);
}
static void clear(){
    int x,y;
    for (x=0;x<vinfo.xres;x++)
        for (y=0;y<vinfo.yres;y++)
        {
            *(fbp+y*finfo.line_length+x*4)=0;
            *(fbp+y*finfo.line_length+x*4+1)=0;
            *(fbp+y*finfo.line_length+x*4+2)=0;
            *(fbp+y*finfo.line_length+x*4+3)=0;
        }

}

static void process_image (const void * p,size_t length){
    fp=fopen("/tmp/video.yuv","a+");
    if(fp == NULL){
        printf("open file error!\n");
        return ;
    }
    fwrite(p,1,length,fp);
    fclose(fp);
/*
  int i,j,r,g,b,y,u,v;
  unsigned char *src=(unsigned char*)p;
  for(i=0;i<480;i++)
  {
  for(j=0;j<640;j++)
  {
  if(j%2)
  {
  y=*(src+i*640*2+j*2) -16;
  u=*(src+i*640*2+j*2-1) -128;
  v=*(src+i*640*2+j*2+1) -128;
  }else
  {
  y=*(src+i*640*2+j*2) -16;
  u=*(src+i*640*2+j*2+1) -128;
  v=*(src+i*640*2+j*2+3) -128;
  }

  r=(298*y+409*v+128)>>8;
  g=(298*y-100*u-208*v+128)>>8;
  b=(298*y+516*u+128)>>8;

  b = (b > 255) ? 255 : ((b < 0) ? 0 : b);
  g = (g > 255) ? 255 : ((g < 0) ? 0 : g);
  r = (r > 255) ? 255 : ((r < 0) ? 0 : r);

  *(fbp+i*finfo.line_length+j*4)=b;

  *(fbp+i*finfo.line_length+j*4+1)=g;
  *(fbp+i*finfo.line_length+j*4+2)=r;
  *(fbp+i*finfo.line_length+j*4+3)=0;
//          fb_putpixel(j,i,RGB(b,g,r));

}
}
*/
//  usleep(20000);

}

static int read_frame (void)
{
    struct v4l2_buffer buf;
    unsigned int i;
    switch (io) {
	case IO_METHOD_READ:
		if (-1 == read (fd, buffers[0].start, buffers[0].length)) {
			switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				errno_exit ("read");
			}
		}
//          process_image (buffers[0].start);
		break;
	case IO_METHOD_MMAP:
		CLEAR (buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		//DQBUF move video buffer to mmap address 
		if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
			printf("errno:%d\n",errno);

			switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				errno_exit ("VIDIOC_DQBUF");
			}
		}
		assert (buf.index < n_buffers);

		process_image (buffers[buf.index].start,buffers[buf.index].length);
		//clean video buffer
		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");
		break;
	case IO_METHOD_USERPTR:
		CLEAR (buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				errno_exit ("VIDIOC_DQBUF");
			}
		}
		for (i = 0; i < n_buffers; ++i)
			if (buf.m.userptr == (unsigned long) buffers[i].start
				&& buf.length == buffers[i].length)
				break;
		assert (i < n_buffers);
        //  process_image ((void *) buf.m.userptr);
		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");
		break;
    }
    return 1;
}

static void mainloop (void)
{
    unsigned int count;
    count = 10;
    while (count-- > 0) {
//  while (1) {
//      CLEAR(t);
//      if(read(key,&t,sizeof(t)) != sizeof(t)){
//          printf("read input fail!\n");
//          break;
//      }
//      if(t.type==EV_KEY)
//          break;
//      
        printf("get frame \n");
        if (read_frame ())
			break;
        sleep(1);       
        /* EAGAIN - continue select loop. */

    }
}
static void stop_capturing (void)
{
    enum v4l2_buf_type type;
    switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;


	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
			errno_exit ("VIDIOC_STREAMOFF");
		break;
    }
}

static void start_capturing (void)
{
    unsigned int i;
    enum v4l2_buf_type type;
    switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		printf("cant read/write!\n");
		break;
	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;
			CLEAR (buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;
			if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");
		break;
	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;
			CLEAR (buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.m.userptr = (unsigned long) buffers[i].start;
			buf.length = buffers[i].length;
			if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
				errno_exit ("VIDIOC_QBUF");
		}


		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");
		break;
    }
}

static void uninit_device (void)
{
    unsigned int i;
    switch (io) {
	case IO_METHOD_READ:
		free (buffers[0].start);
		break;
	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i)
			if (-1 == munmap (buffers[i].start, buffers[i].length))
				errno_exit ("munmap");

		break;
	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i)
			free (buffers[i].start);
		break;
    }
    free (buffers);
}
static void init_read (unsigned int buffer_size)
{
    buffers = (buffer *)calloc (1, sizeof (*buffers));
    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
    buffers[0].length = buffer_size;
    buffers[0].start = malloc (buffer_size);
    if (!buffers[0].start) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
}
/*
 *
 *struct v4l2_requestbuffers {
 *  __u32           count; //number of buff request
 *  __u32           type;  //buf type (v4l2_format)
 *  __u32           memory; // set field to mmap
 *  __u32           reserved[2];
 *};
 *
 * struct v4l2_buffer {
 *  __u32           index;
 *  __u32           type;
 *  __u32           bytesused;
 *  __u32           flags;
 *  __u32           field;
 *  struct timeval      timestamp;
 *  struct v4l2_timecode    timecode;
 *  __u32           sequence;
 *  __u32           memory; //memory location
 *  union {
 *      __u32           offset;
 *      unsigned long   userptr;
 *      struct v4l2_plane *planes;
 *      __s32       fd;
 *  } m;
 *  __u32           length;
 *  __u32           reserved2;
 *  __u32           reserved;
 *};   
 *
 **/
static void init_mmap (void)
{
    struct v4l2_requestbuffers req;
    CLEAR (req);
    req.count = 2;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
					 "memory mapping\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }
    if (req.count < 2) {
        fprintf (stderr, "Insufficient buffer memory on %s\n",
				 dev_name);
        exit (EXIT_FAILURE);
    }
    printf("req buf count:%d\n",req.count);
    buffers = (buffer *)calloc (req.count, sizeof (*buffers));
    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;
        CLEAR (buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;
        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");
        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
				  buf.length,
				  PROT_READ | PROT_WRITE /* required */,
				  MAP_SHARED /* recommended */,
				  fd, buf.m.offset);
        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit ("mmap");
    }
}

static void init_userp (unsigned int buffer_size)
{
    struct v4l2_requestbuffers req;
    CLEAR (req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;
    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
					 "user pointer i/o\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }
    buffers = (buffer *)calloc (4, sizeof (*buffers));
    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        buffers[n_buffers].start = malloc (buffer_size);
        if (!buffers[n_buffers].start) {
            fprintf (stderr, "Out of memory\n");
            exit (EXIT_FAILURE);
        }
    }
}

/*
 *
 *cap --->driver |name of driver
 *    --->card   |name of device
 *    --->bus_info| location of the device in the system
 *    --->version | kernel verison
 *    --->capabilities| 0x00000001 support video capture interface
 *                    | 0x00000002 support video output interface
 *                    | 0x00000004 support store images directly in video memory
 *                    | 0x00020000 device has audio input outputs
 *                    | 0x01000000 support read()/write() I\O methods
 *                    | 0x04000000 support streaming I/O methods
 *
 *cropcap video crop and scaling abilities
 *       ---> type    | V4L2_BUF_TYPE_VIDEO_CAPTURE
 *                    | V4L2_BUF_TYPE_VIDEO_OUTPUT
 *                    | V4L2_BUF_TYPE_VIDEO_OVERLAY
 *       --->(v4l2_rect) bounds | Defines window
 *       --->(v4l2_rect) defrect| default cropping rectangel
 *       --->(v4l2_fract)pixelaspect| pixel aspect(y/x)
 *fmt  vidio format
 *       ---> tpye    |same as above
 *       ---> unio( v4l2_pix_format format_mplane window vbi_format sdr_format raw_data )fmt |set format
 *
 * */

static void init_device (void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;
    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap))
	{
        if (EINVAL == errno) 
        {
            fprintf (stderr, "%s is no V4L2 device\n",dev_name);
            exit (EXIT_FAILURE);
        } else 
        {
            errno_exit ("VIDIOC_QUERYCAP");
        }
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
        fprintf (stderr, "%s is no video capture device\n",dev_name);
        exit (EXIT_FAILURE);
    }
    switch (io) {
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE)) 
		{
			fprintf (stderr, "%s does not support read i/o\n",dev_name);
			exit (EXIT_FAILURE);
		}
		break;
	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING))
		{
			fprintf (stderr, "%s does not support streaming i/o\n",dev_name);
			exit (EXIT_FAILURE);
		}
		break;
    }
    /* Select video input, video standard and tune here. */
    CLEAR (cropcap);
/*  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c = cropcap.defrect; 
	if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) 
	{
	switch (errno)
	{
	case EINVAL:

	printf("Cropping not support!\n");
	break;
	default:
	printf("Cropping faild!\n");

	break;
	}
	}
    } else 
    {
	printf("get cropcap error!\n");
    }
*/
    CLEAR (fmt);
/*
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = 640;
  fmt.fmt.pix.height = 480;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
  if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
  errno_exit ("VIDIOC_S_FMT");*/
    /* Note VIDIOC_S_FMT may change width and height. */
    /* Buggy driver paranoia. */
    /*set bytes per line | image size = width * height *2*/
    /*min = fmt.fmt.pix.width * 2;
	  printf("bytes per line:%d,width*2:%d\n",fmt.fmt.pix.bytesperline,min);
	  if (fmt.fmt.pix.bytesperline < min)
	  fmt.fmt.pix.bytesperline = min;
	  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	  printf("image size:%d,width*height*2:%d\n",fmt.fmt.pix.sizeimage,min);
	  if (fmt.fmt.pix.sizeimage < min)
	  fmt.fmt.pix.sizeimage = min;*/
    switch (io) {
	case IO_METHOD_READ:
		init_read (fmt.fmt.pix.sizeimage);
		break;
	case IO_METHOD_MMAP:
		init_mmap ();
		break;
	case IO_METHOD_USERPTR:
		init_userp (fmt.fmt.pix.sizeimage);
		break;
    }
}

static void close_device (void)
{
    if (-1 == close (fd))
        errno_exit ("close");

}
/*
 *
 *
 * stat  --->st_dev    |device num
 *       --->st_ino    |node    
 *       --->st_mode   |file type read/write permition
 *       --->st_nlink  |file hard link number
 *       --->st_uid gid|user id group ip
 *       --->st_size   |file size byte number
 *       --->st_blksize|file system I/O buffer size
 *       --->st_blocks |block number
 *       --->st_atime ctime mtime |last access, last change property, last modify
 *
 *
 * */

static void open_device (void)
{
    struct stat st;

    if (-1 == stat (dev_name, &st)) {
        fprintf (stderr, "Cannot identify ’%s’: %d, %s\n",
				 dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }

    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
        exit (EXIT_FAILURE);


    }
    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd) {
        fprintf (stderr, "Cannot open ’%s’: %d, %s\n",
				 dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
}
static void usage (FILE * fp,int argc,char ** argv)
{
    fprintf (fp,
			 "Usage: %s [options]\n\n"
			 "Options:\n"
			 "-d | --device name Video device name [/dev/video]\n"
			 "-h | --help Print this message\n"
			 "-m | --mmap Use memory mapped buffers\n"
			 "-r | --read Use read() calls\n"
			 "-u | --userp Use application allocated buffers\n"
			 "",
			 argv[0]);
}
static const char short_options [] = "d:hmru";

static const struct option

long_options [] = {
    { "device", required_argument, NULL, 'd' },
    { "help", no_argument, NULL, 'h' },
    { "mmap", no_argument, NULL, 'm' },
    { "read", no_argument, NULL, 'r' },
    { "userp", no_argument, NULL, 'u' },
    { 0, 0, 0, 0 }
};



int main (int argc,
		  char ** argv)
{
	dev_name = argv[1];

    for (;;) {
        int index;
        int c;
        c = getopt_long (argc, argv,
						 short_options, long_options,
						 &index);
        if (-1 == c)
            break;


		switch (c) {
		case 0: /* getopt_long() flag */
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'h':
			usage (stdout, argc, argv);
			exit (EXIT_SUCCESS);
		case 'm':
			io = IO_METHOD_MMAP;
			break;
		case 'r':
			io = IO_METHOD_READ;
			break;
		case 'u':
			io = IO_METHOD_USERPTR;
			break;
		default:
			usage (stderr, argc, argv);
			exit (EXIT_FAILURE);
		}
    }
    printf("open device\n");
    open_device ();
    printf("init device\n");
    init_device ();
    printf("start_capturing \n");
    start_capturing ();
    printf("mainloop\n");
    mainloop ();
    stop_capturing ();
    clear();
    uninit_device ();
    close_device ();
    exit (EXIT_SUCCESS);
    return 0;
}
