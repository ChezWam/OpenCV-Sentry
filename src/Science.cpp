#include <iostream>
#include <libusb.h>
#include <time.h>
#include <math.h>
#include <opencv2/core/core.hpp>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/legacy/legacy.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <stdio.h>
using namespace cv;
using namespace std;

/*
 * Sample of how OpenCV can be used to interact with the physical world. The turret moves accordingly to what is detected by the webcam.
 * Please be careful, short range nerf shots can be painful.
 */

int main() {

  //openCV init


  CvSize size640x480 = cvSize(640, 480);			// usually supported by most webcams, and allows for a decent framerate on most devices

  CvCapture* p_capWebcam;					// the video stream will be accessed trough this.
  Mat p_imgOriginal;			
  Mat p_imgFace;					

  Mat back;   //backgroundfor mov detec
  Mat fore;   //foreground "	"	"	"

  BackgroundSubtractorMOG2 bg;
  bg.set("nmixtures", 3);
  bg.set("detectShadows", false);
  std::vector<std::vector<cv::Point> > contours;
  cv::namedWindow("Frame");
  cv::namedWindow("Background");

  char charCheckForEscKey;			// pressing escape will kill the program

  p_capWebcam = cvCaptureFromCAM(0);		// 0 is the first camera detected on the computer. Set accordingly if you have several.

  if(p_capWebcam == NULL) {			// capture failure, check if your cam is not already in use
	  printf("error: capture is NULL \n");	// 
	  getchar();								// dirty way of pausing execution
	  return(-1);								// exit program
  }

  cvNamedWindow("Original", CV_WINDOW_AUTOSIZE);		// unmodified image
	  
  p_imgFace = cvCreateImage(size640x480,			//kept the same size from previous var
		    IPL_DEPTH_8U,		// 8-bit color depth
		    1);			//matrix type, see slides

//libusb init

  libusb_device **devs;				//pointer to pointer of device, used to retrieve a list of devices
  libusb_device_handle *dev_handle; 
  libusb_context *ctx = NULL; 			//a libusb session
  int r;					//for return values
  ssize_t cnt; 					//holding number of devices in list
  r = libusb_init(&ctx); 			//initialize the library for the session we just declared
  if(r < 0) {
	  cout<<"Init Error "<<r<<endl; 	//failure.
	  return 1;
  }
  libusb_set_debug(ctx, 3); 			//verbosity level to 3, as suggested in various examples

  cnt = libusb_get_device_list(ctx, &devs); 	//get the list of devices
  if(cnt < 0) {
	  cout<<"Get Device Error"<<endl; 	//failure
	  return 1;
  }
  cout<<cnt<<" Devices in list."<<endl;

  dev_handle = libusb_open_device_with_vid_pid(ctx, 8483, 4112);	 //vendorID & productID. Will be unique to that type of turret. use lsusb or  dmesg  after plugin your device to find yours.
  if(dev_handle == NULL)
	  cout<<"Cannot open device"<<endl;
  else
	  cout<<"Device Opened"<<endl;
  libusb_free_device_list(devs, 1); // no use of keeping what's plugged on the computer

  unsigned char *data = new unsigned char[8]; //data we'll write trough USB
  data[0]=2;data[1]=16;data[2]=0;data[3]=0;data[4]=0;data[5]=0;data[6]=0;data[7]=0; //init values or expect a freeze.
  //each array stores 8 bytes, which are the commands that the turret will recognize.  reversed listening to the usb bus the device was plugged in.
  unsigned char *fireZeMissiles = new unsigned char[8];
  fireZeMissiles[0]=2;fireZeMissiles[1]=16;fireZeMissiles[2]=0;fireZeMissiles[3]=0;fireZeMissiles[4]=0;fireZeMissiles[5]=0;fireZeMissiles[6]=0;fireZeMissiles[7]=0;

  unsigned char *up = new unsigned char[8];
  up[0]=2;up[1]=2;up[2]=0;up[3]=0;up[4]=0;up[5]=0;up[6]=0;up[7]=0; 

  unsigned char *down = new unsigned char[8];
  down[0]=2;down[1]=1;down[2]=0;down[3]=0;down[4]=0;down[5]=0;down[6]=0;down[7]=0; 

  unsigned char *left = new unsigned char[8];
  left[0]=2;left[1]=4;left[2]=0;left[3]=0;left[4]=0;left[5]=0;left[6]=0;left[7]=0;

  unsigned char *right = new unsigned char[8];
  right[0]=2;right[1]=8;right[2]=0;right[3]=0;right[4]=0;right[5]=0;right[6]=0;right[7]=0; 

  unsigned char *stop = new unsigned char[8];
  stop[0]=2;stop[1]=32;stop[2]=0;stop[3]=0;stop[4]=0;stop[5]=0;stop[6]=0;stop[7]=0; 


  if(libusb_kernel_driver_active(dev_handle, 0) == 1) { //find out if kernel driver is attached
	  cout<<"Kernel Driver Active"<<endl;
	  if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
		  cout<<"Kernel Driver Detached!"<<endl;
  }
  r = libusb_claim_interface(dev_handle, 0); //claim interface 0 of the device. The turret only has one.
  if(r < 0) {
	  cout<<"Cannot Claim Interface"<<endl; //aka, already in use.
	  return 1;
  }
  cout<<"Claimed Interface"<<endl;

  cout<<"Data->"<<data<<"<-"<<endl;
  cout<<"Writing Data..."<<endl;

  CascadeClassifier cascade;  //haarcascade are a simple, if not the most efficient, way of detecting physical objects. Here, faces.
  cascade.load("haarcascade.xml");

  vector<Rect> faces;

  bool manualOverride = true;  //we start in manual mode, move the turret with your keyboad
  bool trackingMovement = false; //track movement or faces
  int engagementRules = 0; //will not autofire by default if detecting movement or someone.
  //bool lastTravelled = true;
  //int patrolCount = 0;

  String control = "Manual Control";
  String engagement = "Rules : Tracking targets";
  String trackingString = "Tracking : movement";



  while(1)
  {								// for each frame . . .
	  p_imgOriginal = cvQueryFrame(p_capWebcam);		// get frame from webcam
	  cvtColor(p_imgOriginal, p_imgFace, CV_BGR2GRAY);	// as most webcam lack a default greymode, we convert to greyscale as color is not that useful.


      if(!manualOverride)					//automated stuff
      {

	  if (!trackingMovement)
	  {
	  equalizeHist( p_imgFace, p_imgFace );
	  cascade.detectMultiScale( p_imgFace, faces, 1.1, 2, 0|CV_HAAR_SCALE_IMAGE, Size(30, 30) );

	    Point closestFaceCenter;
		  int MaxHeight = 0;
	    for ( unsigned int i = 0; i < faces.size(); i++)  //for each detected face, draw a rectangle around it. biggest is the closest one, aka the targetted one
						    {
							Rect r = faces[i];
							rectangle(p_imgOriginal, cvPoint(r.x, r.y), cvPoint(r.x + r.width, r.y + r.height), CV_RGB(0,255,0), 1);
							if (r.height > MaxHeight)
							{
							    MaxHeight = r.height;
							    closestFaceCenter = Point(r.x, r.y);
							}
						    }

	    cout << "face at x : " << closestFaceCenter.x << " y : "<< closestFaceCenter.y <<endl;
	    cout << "middle at x : " << p_imgOriginal.rows<< " y : "<< p_imgOriginal.cols << endl;

	    if((closestFaceCenter.x - (p_imgOriginal.cols/2)) < 70  && closestFaceCenter.x != 0 && engagementRules == 1) //if the turret is pointing towards the target, fire
	    {
		      libusb_control_transfer(dev_handle,33, 9, 0, 0, fireZeMissiles, 8, 0); 


	    }
	    //correct aim if a face is detected.
	    if (closestFaceCenter.x < p_imgOriginal.rows/2 && closestFaceCenter.x != 0  && (p_imgOriginal.cols/2 - closestFaceCenter.x) > 70)
	    {
		    libusb_control_transfer(dev_handle,33, 9, 0, 0, left, 8, 0);
			  struct timespec tim, tim2;
			      tim.tv_sec = 0;		//time before executing sleep
			      tim.tv_nsec = 30000000L;  //the turret executes last command until a stop command is sent, so wait during moves.

			      nanosleep(&tim , &tim2);

			  libusb_control_transfer(dev_handle,33, 9, 0, 0, stop, 8, 0);
	    }

	    else if (closestFaceCenter.x > p_imgOriginal.cols/2 && (closestFaceCenter.x - p_imgOriginal.cols/2) > 70)
	    {
		    libusb_control_transfer(dev_handle,33, 9, 0, 0, right, 8, 0);
			  struct timespec tim, tim2;
			      tim.tv_sec = 0;
			      tim.tv_nsec = 30000000L;

			      nanosleep(&tim , &tim2);

			  libusb_control_transfer(dev_handle,33, 9, 0, 0, stop, 8, 0);
	    }
	    }
	    else
	    {

			    bg.operator()(p_imgOriginal,fore);
			    bg.getBackgroundImage(back);
			    cv::erode(fore,fore,cv::Mat());
			    cv::dilate(fore,fore,cv::Mat());
			    cv::findContours(fore,contours,CV_RETR_EXTERNAL,CV_CHAIN_APPROX_NONE);
			    cv::drawContours(p_imgFace,contours,-1,cv::Scalar(0,0,255),2);
			    cv::imshow("Frame",p_imgFace);
			    cv::imshow("Background",back);
	    }

	    }


	  //application text
	    putText(p_imgOriginal, control, cvPoint(30,30), FONT_HERSHEY_SIMPLEX, 0.5, Scalar::all(255), 1,8);
	    putText(p_imgOriginal, engagement, cvPoint(30,45), FONT_HERSHEY_SIMPLEX, 0.5, Scalar::all(255), 1,8);
	    putText(p_imgOriginal, trackingString, cvPoint(30,60), FONT_HERSHEY_SIMPLEX, 0.5, Scalar::all(255), 1,8);

	    imshow("Original", p_imgOriginal);			



	  charCheckForEscKey = cvWaitKey(5);			
	  if(charCheckForEscKey == 109)
	  {


		  manualOverride ^= true;
		  switch (manualOverride)
		  {
			case false: {
						  control = "Automatic control";
						  break;
		      }
		      case true:
		      {
						  control = "Manual control";
						  break;
		      }
		  }

	  }
	  if(charCheckForEscKey == 116)
	  {
		  trackingMovement ^= true;
		  if (trackingMovement == true)
		  {
			  trackingString = "Tracking : movement";
		  }
		  else
		  {
			  trackingString = "Tracking : faces";
		  }
	  }
	  if(charCheckForEscKey == 101)
	  {
		  if(engagementRules == 2)
			  {
				  engagementRules = 0;
			  }
		  else if(engagementRules < 3)
		  {
			  engagementRules++;
		  }

	  }
	  switch (engagementRules)
	  {
		  case 0:
		  {
			    engagement = "Rules : Tracking targets";
			    break;
		  }
		  case 1:
		  {
			    engagement = "Rules : Fire at will";
			    break;
		  }
		  case 2:
		  {
			    engagement = "Rules : Stay idle";
			    break;
		  }
	  }
	  if(manualOverride)
	  {
		  switch (charCheckForEscKey)
					  {
						case 113:
						{
							  libusb_control_transfer(dev_handle,33, 9, 0, 0, left, 8, 0);
							  break;
						}
						case 122:
						{
								  libusb_control_transfer(dev_handle,33, 9, 0, 0, up, 8, 0);
								  break;
						}
						case 100:
						{
								  libusb_control_transfer(dev_handle,33, 9, 0, 0, right, 8, 0);
								  break;
						}
						case 115:
						{
								  libusb_control_transfer(dev_handle,33, 9, 0, 0, down, 8, 0);
								  break;
						}
						case 32:
						{
								  libusb_control_transfer(dev_handle,33, 9, 0, 0, fireZeMissiles, 8, 0);
								  break;
						    }

					  }
		  if(charCheckForEscKey == 113 || charCheckForEscKey== 122 || charCheckForEscKey == 100 || charCheckForEscKey== 115)
		  {
			  struct timespec tim, tim2;
						      tim.tv_sec = 0;
						      tim.tv_nsec = 50000000L;

						      nanosleep(&tim , &tim2);

						  libusb_control_transfer(dev_handle,33, 9, 0, 0, stop, 8, 0);
		  }
	  }
	  if(charCheckForEscKey == 27) break;				// if Esc jump out ofloop
  }



  cvReleaseCapture(&p_capWebcam);					// release memory as applicable
  cvDestroyWindow("Original");
  r = libusb_release_interface(dev_handle, 0); //release the claimed interface
  if(r!=0) {
	  cout<<"Cannot Release Interface"<<endl;
	  return 1;
  }
  cout<<"Released Interface"<<endl;

  libusb_close(dev_handle); //close the device we opened
  libusb_exit(ctx); //needs to be called to end the session

  delete[] data; //delete the allocated memory 
  return 0;
}
