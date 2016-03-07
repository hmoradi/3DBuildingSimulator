##include <windows.h>		// Header File For Windows
#include <iostream>
#include <math.h>			// Math Library Header File
#include <stdio.h>			// Header File For Standard Input/Output
#include <gl\gl.h>			// Header File For The OpenGL32 Library
#include <gl\glu.h>			// Header File For The GLu32 Library
#include <gl\glaux.h>		// Header File For The Glaux Library
using namespace std;
typedef struct point_3d {			// Structure for a 3-dimensional point (NEW)
	double x, y, z;
} POINT_3D;

typedef struct bpatch {				// Structure for a 3rd degree bezier patch (NEW)
	POINT_3D	anchors[4][4];			// 4x4 grid of anchor points
	GLuint		dlBPatch;				// Display List for Bezier Patch
	GLuint		texture;                // Texture for the patch
	char* filename;
	float A;
	float B;
	float C;
	float D;
} BEZIER_PATCH;

HDC			hDC=NULL;		// Private GDI Device Context
HGLRC		hRC=NULL;		// Permanent Rendering Context
HWND		hWnd=NULL;		// Holds Our Window Handle
HINSTANCE	hInstance;		// Holds The Instance Of The Application
GLdouble eqn[4] = {0.0, 1.0, 0.0, -20.0};
bool	keys[256];			// Array Used For The Keyboard Routine
bool	active=TRUE;		// Window Active Flag Set To TRUE By Default
bool	fullscreen=TRUE;	// Fullscreen Flag Set To Fullscreen Mode By Default
bool	blend;				// Blending ON/OFF
bool	bp;					// B Pressed?
bool	fp;
bool flag=1;
float wall_length = 15.3;
GLUquadricObj *quadratic;
GLuint  object=0;
float up_down_speed = 0.25;
// F Pressed?
int numtriangles;
BEZIER_PATCH	mybezier;			// The bezier patch we're going to use (NEW)
BEZIER_PATCH	mybezier1;
BEZIER_PATCH * world;
BOOL			showCPoints=TRUE;	// Toggles displaying the control point grid (NEW)
int				divs = 7;			// Number of intrapolations (conrols poly resolution) (NEW)
const float piover180 = 0.0174532925f;
float heading;
float xpos;
float zpos;
float lx=0,ly=0,lz=0,cx=0,cy=0,cz=0,yt=0;
float angle = 0;
GLfloat	yrot;				// Y Rotation
GLfloat walkbias = 0;
GLfloat walkbiasangle = 0;
GLfloat lookupdown = 0.0f;
GLfloat	z=0.0f;				// Depth Into The Screen
GLfloat ytemp=5.0f;
GLuint	filter;				// Which Filter To Use
GLuint	texture[6];	//3		// Storage For 3 Textures
GLfloat ambient[] = { 1.0, 1.0, 1.0, 1.0 };
GLfloat diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
GLfloat position[] = { 0.0, 7.0, -5.0, 1.0 };
GLfloat lmodel_ambient[] = { 0.4, 0.4, 0.4, 1.0 };
GLfloat local_view[] = { 0.0 };
LRESULT	CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);	// Declaration For WndProc
// Adds 2 points. Don't just use '+' ;)
POINT_3D pointAdd(POINT_3D p, POINT_3D q) {
	p.x += q.x;		p.y += q.y;		p.z += q.z;
	return p;
}

// Multiplies a point and a constant. Don't just use '*'
POINT_3D pointTimes(double c, POINT_3D p) {
	p.x *= c;	p.y *= c;	p.z *= c;
	return p;
}

// Function for quick point creation
POINT_3D makePoint(double a, double b, double c) {
	POINT_3D p;
	p.x = a;	p.y = b;	p.z = c;
	return p;
}
POINT_3D Bernstein(float u, POINT_3D *p) {
	POINT_3D	a, b, c, d, r;

	a = pointTimes(pow(u,3), p[0]);
	b = pointTimes(3*pow(u,2)*(1-u), p[1]);
	c = pointTimes(3*u*pow((1-u),2), p[2]);
	d = pointTimes(pow((1-u),3), p[3]);

	r = pointAdd(pointAdd(a, b), pointAdd(c, d));

	return r;
}
BOOL LoadGLTexture(GLuint *texPntr, char* name)
{
	BOOL success = FALSE;
	AUX_RGBImageRec *TextureImage = NULL;

	glGenTextures(1, texPntr);						// Generate 1 texture

	FILE* test=NULL;
	TextureImage = NULL;

	test = fopen(name, "r");						// test to see if the file exists
	if (test != NULL) {								// if it does
		fclose(test);									// close the file
		TextureImage = auxDIBImageLoad(name);			// and load the texture
	}

	if (TextureImage != NULL) {						// if it loaded
		success = TRUE;

		// Typical Texture Generation Using Data From The Bitmap
		glBindTexture(GL_TEXTURE_2D, *texPntr);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, TextureImage->sizeX, TextureImage->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, TextureImage->data);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	}

	if (TextureImage->data)
		free(TextureImage->data);

	return success;
}
void readstr(FILE *f,char *string)
{
	do
	{
		fgets(string, 255, f);
	} while ((string[0] == '/') || (string[0] == '\n'));
	return;
}
GLuint genBezier(BEZIER_PATCH patch, int divs) {
	int			u = 0, v;
	float		py, px, pyold; 
	GLuint		drawlist = glGenLists(1);		// make the display list
	POINT_3D	temp[4];
	POINT_3D	*last = (POINT_3D*)malloc(sizeof(POINT_3D)*(divs+1));
	// array of points to mark the first line of polys

	if (patch.dlBPatch != NULL)					// get rid of any old display lists
		glDeleteLists(patch.dlBPatch, 1);

	temp[0] = patch.anchors[0][3];				// the first derived curve (along x axis)
	temp[1] = patch.anchors[1][3];
	temp[2] = patch.anchors[2][3];
	temp[3] = patch.anchors[3][3];

	for (v=0;v<=divs;v++) {						// create the first line of points
		px = ((float)v)/((float)divs);			// percent along y axis
		// use the 4 points from the derives curve to calculate the points along that curve
		last[v] = Bernstein(px, temp);
	}

	glNewList(drawlist, GL_COMPILE);				// Start a new display list
	glBindTexture(GL_TEXTURE_2D, patch.texture);	// Bind the texture

	for (u=1;u<=divs;u++) {
		py	  = ((float)u)/((float)divs);			// Percent along Y axis
		pyold = ((float)u-1.0f)/((float)divs);		// Percent along old Y axis

		temp[0] = Bernstein(py, patch.anchors[0]);	// Calculate new bezier points
		temp[1] = Bernstein(py, patch.anchors[1]);
		temp[2] = Bernstein(py, patch.anchors[2]);
		temp[3] = Bernstein(py, patch.anchors[3]);

		glBegin(GL_TRIANGLE_STRIP);					// Begin a new triangle strip

		for (v=0;v<=divs;v++) {
			px = ((float)v)/((float)divs);			// Percent along the X axis

			glTexCoord2f(pyold, px);				// Apply the old texture coords
			glVertex3d(last[v].x, last[v].y, last[v].z);	// Old Point

			last[v] = Bernstein(px, temp);			// Generate new point
			glTexCoord2f(py, px);					// Apply the new texture coords
			glVertex3d(last[v].x, last[v].y, last[v].z);	// New Point
		}

		glEnd();									// END the triangle srip
	}

	glEndList();								// END the list

	free(last);									// Free the old vertices array
	return drawlist;							// Return the display list
}
void SetupWorld()
{
	float x1, y1, z1, x2 , y2 , z2 , x3 , y3, z3 , x4, y4, z4 ;
	//int numtriangles;
	FILE *filein;
	char oneline[255];
	filein = fopen("data/world.txt", "rt");				// File To Load World Data From

	readstr(filein,oneline);
	sscanf(oneline, "NUMPOLLIES %d\n", &numtriangles);


	world = new BEZIER_PATCH[numtriangles];
	//for(int k=0;k < numtriangles;k++)
		
	for (int loop = 0; loop < numtriangles; loop++)
	{
		for (int vert = 0; vert < 4; vert++)
		{
			readstr(filein,oneline);
			sscanf(oneline, "%f %f %f %f %f %f %f %f %f %f %f %f" ,&x1, &y1, &z1,&x2, &y2, &z2,&x3, &y3, &z3 , &x4, &y4, &z4);
			world[loop].anchors[vert][0] = makePoint(x1,y1,z1);
			world[loop].anchors[vert][1] = makePoint(x2,y2,z2);
			world[loop].anchors[vert][2] = makePoint(x3,y3,z3);
			world[loop].anchors[vert][3] = makePoint(x4,y4,z4);
			world[loop].dlBPatch = NULL;
		}
		readstr(filein,oneline);
		sscanf(oneline, "%f %f %f %f", &world[loop].A,&world[loop].B,&world[loop].C,&world[loop].D);
		readstr(filein,oneline);
		world[loop].filename = new char[strlen(oneline)];
		sscanf(oneline, "%s", world[loop].filename);
		LoadGLTexture(&(world[loop].texture),world[loop].filename );	// Load the texture
		world[loop].dlBPatch = genBezier(world[loop], divs);
	}
	fclose(filein);
	return;
}


/************************************************************************************/




AUX_RGBImageRec *LoadBMP(char *Filename)                // Loads A Bitmap Image
{
	FILE *File=NULL;                                // File Handle

	if (!Filename)                                  // Make Sure A Filename Was Given
	{
		return NULL;                            // If Not Return NULL
	}

	File=fopen(Filename,"r");                       // Check To See If The File Exists

	if (File)                                       // Does The File Exist?
	{
		fclose(File);                           // Close The Handle
		return auxDIBImageLoad(Filename);       // Load The Bitmap And Return A Pointer
	}
	return NULL;                                    // If Load Failed Return NULL
}

int LoadGLTextures()                                    // Load Bitmaps And Convert To Textures
{
	int Status=FALSE;                               // Status Indicator

	AUX_RGBImageRec *TextureImage[6]; // 1==3==4               // Create Storage Space For The Texture

	memset(TextureImage,0,sizeof(void *)*1);        // Set The Pointer To NULL

	// Load The Bitmap, Check For Errors, If Bitmap's Not Found Quit
	if ((TextureImage[0]=LoadBMP("Data/wall4.BMP")) && (TextureImage[1]=LoadBMP("Data/soton1.BMP"))&&(TextureImage[2]=LoadBMP("Data/kaf9.BMP"))&&(TextureImage[3]=LoadBMP("Data/aye.BMP"))&&(TextureImage[4]=LoadBMP("Data/moket1.BMP"))&&(TextureImage[5]=LoadBMP("Data/gonbad.BMP")))
	{
		Status=TRUE;                            // Set The Status To TRUE

		glGenTextures(6, &texture[0]);    //3=4      // Create Three Textures

		// Create Nearest Filtered Texture
		glBindTexture(GL_TEXTURE_2D, texture[0]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);           
 		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, TextureImage[0]->sizeX, TextureImage[0]->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, TextureImage[0]->data);

		// Create Linear Filtered Texture
		glBindTexture(GL_TEXTURE_2D, texture[1]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, TextureImage[1]->sizeX, TextureImage[1]->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, TextureImage[1]->data);

		// Create MipMapped Texture
		glBindTexture(GL_TEXTURE_2D, texture[2]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, 3, TextureImage[2]->sizeX, TextureImage[2]->sizeY, GL_RGB, GL_UNSIGNED_BYTE, TextureImage[2]->data);
	///******************************	
		glBindTexture(GL_TEXTURE_2D, texture[3]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, 3, TextureImage[3]->sizeX, TextureImage[3]->sizeY, GL_RGB, GL_UNSIGNED_BYTE, TextureImage[3]->data);
		
		glBindTexture(GL_TEXTURE_2D, texture[4]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, 3, TextureImage[4]->sizeX, TextureImage[4]->sizeY, GL_RGB, GL_UNSIGNED_BYTE, TextureImage[4]->data);
		
		glBindTexture(GL_TEXTURE_2D, texture[5]);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);
		gluBuild2DMipmaps(GL_TEXTURE_2D, 3, TextureImage[5]->sizeX, TextureImage[5]->sizeY, GL_RGB, GL_UNSIGNED_BYTE, TextureImage[5]->data);
	}
	if (TextureImage[0])                            // If Texture Exists
	{
		if (TextureImage[0]->data)              // If Texture Image Exists
		{
			free(TextureImage[0]->data);    // Free The Texture Image Memory
		}

		free(TextureImage[0]);                  // Free The Image Structure
	}
	if (TextureImage[1])                            // If Texture Exists
	{
		if (TextureImage[1]->data)              // If Texture Image Exists
		{
			free(TextureImage[1]->data);    // Free The Texture Image Memory
		}

		free(TextureImage[1]);                  // Free The Image Structure
	}
	if (TextureImage[2])                            // If Texture Exists
	{
		if (TextureImage[2]->data)              // If Texture Image Exists
		{
			free(TextureImage[2]->data);    // Free The Texture Image Memory
		}

		free(TextureImage[2]);                  // Free The Image Structure
	}
	if (TextureImage[3])                            // If Texture Exists
	{
		if (TextureImage[3]->data)              // If Texture Image Exists
		{
			free(TextureImage[3]->data);    // Free The Texture Image Memory
		}

		free(TextureImage[3]);                  // Free The Image Structure
	}
	if (TextureImage[4])                            // If Texture Exists
	{
		if (TextureImage[4]->data)              // If Texture Image Exists
		{
			free(TextureImage[4]->data);    // Free The Texture Image Memory
		}

		free(TextureImage[4]);                  // Free The Image Structure
	}
	return Status;                                  // Return The Status
}

GLvoid ReSizeGLScene(GLsizei width, GLsizei height)		// Resize And Initialize The GL Window
{
	if (height==0)										// Prevent A Divide By Zero By
	{
		height=1;										// Making Height Equal One
	}

	glViewport(0,0,width,height);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix

	// Calculate The Aspect Ratio Of The Window
	gluPerspective(45.0f,(GLfloat)width/(GLfloat)height,0.1f,100.0f);

	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();									// Reset The Modelview Matrix
}
GLvoid glDrawCube()
{
		glBegin(GL_QUADS);
		// Front Face
	///	glNormal3f( 0.0f, 0.0f, 1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
		// Back Face
//		glNormal3f( 0.0f, 0.0f,-1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
		// Top Face
//		glNormal3f( 0.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
		// Bottom Face
//		glNormal3f( 0.0f,-1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
		// Right Face
//		glNormal3f( 1.0f, 0.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
		// Left Face
//		glNormal3f(-1.0f, 0.0f, 0.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
	glEnd();
}
int InitGL(GLvoid)										// All Setup For OpenGL Goes Here
{
	if (!LoadGLTextures())								// Jump To Texture Loading Routine
	{
		return FALSE;									// If Texture Didn't Load Return FALSE
	}
	glEnable(GL_TEXTURE_2D);							// Enable Texture Mapping
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);					// Set The Blending Function For Translucency
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);				// This Will Clear The Background Color To Black
	glClearDepth(1.0);									// Enables Clearing Of The Depth Buffer
	glDepthFunc(GL_LESS);								// The Type Of Depth Test To Do
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glShadeModel(GL_SMOOTH);							// Enables Smooth Color Shading
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations*	glLightfv(GL_LIGHT0, GL_AMBIENT, LightAmbient);		// Setup The Ambient Light
	glEnable(GL_DEPTH_TEST);
//	glDepthFunc(GL_LESS);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, local_view);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	quadratic=gluNewQuadric();							// Create A Pointer To The Quadric Object (Return 0 If No Memory) (NEW)
	gluQuadricNormals(quadratic, GLU_SMOOTH);			// Create Smooth Normals (NEW)
	gluQuadricTexture(quadratic, GL_TRUE);				// Create Texture Coords (NEW)

	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	//cout << "hesam"<<endl;
	SetupWorld();
	return TRUE;										// Initialization Went OK
}

int DrawGLScene(GLvoid)									// Here's Where We Do All The Drawing
{
	if(flag==1)
	{
		flag=0;
	//	PlaySound("Data/azan2.wav", NULL, SND_ASYNC);
		
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear The Screen And The Depth Buffer
	glLoadIdentity();									// Reset The View
	GLfloat no_mat[] = { 0.0, 0.0, 0.0, 1.0 };
	GLfloat mat_ambient[] = { 1.7, 1.7, 1.7, 1.0 };
	GLfloat mat_ambient_color[] = { 0.8, 0.8, 0.2, 1.0 };
	GLfloat mat_diffuse[] = { 0.1, 0.5, 0.8, 1.0 };
	GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat no_shininess[] = { 0.0 };
	GLfloat low_shininess[] = { 1.0 };
	GLfloat high_shininess[] = { 100.0 };
	GLfloat mat_emission[] = {0.3, 0.2, 0.2, 0.0};

	GLfloat x_m, y_m, z_m, u_m, v_m;
	GLfloat xtrans = -xpos;
	GLfloat ztrans = -zpos;
	GLfloat ytrans = -walkbias-0.25f;
	ytrans-=ytemp;
	GLfloat sceneroty = 360.0f - yrot;
	glRotatef(lookupdown,1.0f,0,0);
	glRotatef(sceneroty,0,1.0f,0);
	glTranslatef(xtrans, ytrans, ztrans);
	//glPointSize(10.0);
	//glColor3f(1.0, 0.0, 0.0);
	//glBegin(GL_POINTS);
	//glVertex3f(position[0],position[1],position[2]);
	//glEnd();
	glColor3f(1.0, 1.0, 1.0);

	// Process Each Triangle
	for (int loop_m = 0; loop_m < numtriangles; loop_m++)
	{
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glCallList(world[loop_m].dlBPatch);
	}
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTranslatef(-5.0,6.0,-3.0);
	glScalef(1.0,6.0,1.5);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		///soton sardar chap
	glScalef(1.0,(float)(1.0/6.0),0.66);
	glTranslatef(5.0, -6.0,3.0);
	glTranslatef(5.0,6.0,-3.0);
	glScalef(1.0,6.0,1.5);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();     ///soton sardar rast
	glScalef(1.0,(float)(1.0/6.0),-0.66);
	glTranslatef(-5.0,-6.0,3.0);
	
	glTranslatef(-5.0,6.0,10.0);
	glScalef(1.0,6.0,1.5);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		///soton sardar chap aghab
	glScalef(1.0,(float)(1.0/6.0),0.66);
	glTranslatef(5.0,-6.0,-10.0);

	glTranslatef(5.0,6.0,10.0);
	glScalef(1.0,6.0,1.5);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		///soton sardar rast aghab
	glScalef(1.0,(float)(1.0/6.0),0.66);
	glTranslatef(-5.0,-6.0,-10.0);
	
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	for( int o=0; o < 4 ; o++)   // soton haye samt rast jolo
	{
		glTranslatef(7.0+(float)(o*15.3),12.0,-3.15);
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(-(7.0+(float)(o*15.3)),-12.0,3.15);
	}
	
	for( int o=0; o < 4 ; o++)   //soton heye samt rast aghab
	{
		glTranslatef(7.0+(float)(o*15.3),12.0,10.15);
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(-(7.0+(float)(o*15.3)),-12.0,-10.15);
	}
	for(int o=0; o < 4 ; o++)   //soton heye samt chap aghab
	{
		glTranslatef(-(7.0+(float)(o*15.3)),12.0,10.15);
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef((7.0+(float)(o*15.3)),-12.0,-10.15);
	}
	for( int o=0; o < 4 ; o++)   // soton haye samt chap jolo
	{
		glTranslatef(-(7.0+(float)(o*15.3)),12.0,-3.15);
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef((7.0+(float)(o*15.3)),-12.0,3.15);
	}
	
	for( int o = 2; o < 7 ; o++)   // soton haye samt chap divar samt chap
	{
		glTranslatef(-52.9,12.0,(-1.15+(float)(o*(wall_length))));
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(52.9,-12.0,-(-1.15+(float)(o*(wall_length))));
	}
	for(int o = 2; o < 7 ; o++)   // soton haye samt chap divar samt rast
	{
		glTranslatef(52.9,12.0,(-1.15+(float)(o*(wall_length))));
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(-52.9,-12.0,-(-1.15+(float)(o*(wall_length))));
	}

	for(int o = 2; o < 7 ; o++)   // soton haye samt chap divar samt rast   
	{
		glTranslatef(-36.0,12.0,(-1.15+(float)(o*(wall_length))));
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(36,-12.0,-(-1.15+(float)(o*(wall_length))));
	}
	for( int o = 2; o < 7 ; o++)   // soton haye samt chap divar samt rast
	{
		glTranslatef(36,12.0,(-1.15+(float)(o*(wall_length))));
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(-36.0,-12.0,-(-1.15+(float)(o*(wall_length))));
	}
	for(int  o = 0; o < 7 ; o++)   // soton haye samt chap divar samt rast
	{
		glTranslatef(85.9,12.0,(-1.15+(float)(o*(wall_length))));
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(-85.9,-12.0,-(-1.15+(float)(o*(wall_length))));
	}
	//////////****************//////////////
	glTranslatef(12.75,17.0,-10.0);  ////goldaseteh
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,2.0125f,2.0125f,12.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(-12.5,-17.0,10.0);

	glTranslatef(-12.75,17.0,-10.0); //goldasteh
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,2.0125f,2.0125f,12.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(12.5,-17.0,10.0);
	
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	for(int h = 1; h < 6;h++)
	{
		glTranslatef(5.0+(float)(h*5.0),14.0,5.0); //saghf samt rast hayat
		glScalef(5.0,2.0,10.0);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glDrawCube();		
		glScalef((float)(1.0/5.0),0.5,(float)(1.0/10.0));
		glTranslatef(-(5.0+(float)(h*5.0)),-14.0,-5.0);
	}
	for(int h =1 ; h < 6 ; h++)
	{
		glTranslatef(-(5.0+(float)(h*5.0)),14.0,5.0); //saghf samt chap hayat
		glScalef(5.0,2.0,10.0);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glDrawCube();		
		glScalef((float)(1.0/5.0),0.5,(float)(1.0/10.0));
		glTranslatef((5.0+(float)(h*5.0)),-14.0,-5.0);
	}
	
	for(int h=0;h<18;h++)
	{
		glTranslatef(44.0,14.0,5.0+(float)(h*5.0)); //saghf samt rast hayat 
		glScalef(11.0,2.0,5.0);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glDrawCube();		
		glScalef((float)(1.0/11.0),0.5,(float)(1.0/5.0));
		glTranslatef(-44.0,-14.0,-(5.0+(float)(h*5.0)));
	}
	for(int h=0;h<18;h++)
	{
		glTranslatef(-44.0,14.0,5.0+(float)(h*5.0)); //saghf samt chap hayat
		glScalef(11.0,2.0,5.0);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glDrawCube();		
		glScalef((float)(1.0/11.0),0.5,(float)(1.0/5.0));
		glTranslatef(44.0,-14.0,-(5.0+(float)(h*5.0)));
	}
	for(int h=0;h<18;h++)
	{
		glTranslatef(70.0,14.0,5.0+(float)(h*5.0)); //saghf samt rast hayat  bironi 
		glScalef(20.0,2.0,5.0);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glDrawCube();		
		glScalef((float)(1.0/20.0),0.5,(float)(1.0/5.0));
		glTranslatef(-70.0,-14.0,-(5.0+(float)(h*5.0)));
	}
	
	
	glBindTexture(GL_TEXTURE_2D, texture[2]); ///kaf posh
	for(int r=0;r < 10;r++)
	{
		for(int t =0;t < 14;t++)
		{
			glTranslatef((-47.0+(float)(t*10.0)),0.0,(3.0+(float)(r*10.0))); //saghf samt chap hayat
			glScalef(5.0,0.01,5.0);
			glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
			glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
			glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
			glDrawCube();		
			glScalef((float)(1.0/5.0),100.0,(float)(1.0/5.0));
			glTranslatef(-(-47.0+(float)(t*10.0)),0.0,-(3.0+(float)(r*10.0)));
		}
	}

	glBindTexture(GL_TEXTURE_2D, texture[4]); ///kaf posh
	for(int  r=0;r < 20;r++)
	{
		for(int t =0;t < 21;t++)
		{
			glTranslatef((-50.0+(float)(t*5.0)),0.0,(93.0+(float)(r*5.0))); //saghf samt chap hayat
			glScalef(2.5,0.01,2.5);
			glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
			glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
			glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
			glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
			glDrawCube();		
			glScalef((float)(1.0/2.5),100.0,(float)(1.0/2.5));
			glTranslatef(-(-50.0+(float)(t*5.0)),0.0,-(93.0+(float)(r*5.0)));
		}
	}

	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTranslatef(-29.0,10.0,92.0); //nim divar jolo chap
	glScalef(24.0,10.0,0.01);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef((float)(1.0/24.0),(float)(1.0/10.0),100.0);
	glTranslatef(29.0,-10.0,-92.0);
	
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTranslatef(29.0,10.0,92.0); //nim divar jolo rast
	glScalef(24.0,10.0,0.01);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef((float)(1.0/24.0),(float)(1.0/10.0),100.0);
	glTranslatef(-29.0,-10.0,-92.0);
	
	glTranslatef(-52.9,10.0,145.0); //davar asli masjed chap
	glScalef(0.01,10.0,53.0);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef(100.0,(float)(1.0/10.0),(float)(1.0/53.0));
	glTranslatef(52.9,-10.0,-145.0);

	glTranslatef(52.9,10.0,145.0); //divar asli masjed samt rast
	glScalef(0.01,10.0,53.0);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef(100.0,(float)(1.0/10.0),(float)(1.0/53.0));
	glTranslatef(-52.9,-10.0,-145.0);
	
	glTranslatef(0.0,10.0,198); //divar asli masjed samt aghab
	glScalef(53.0,10.0,0.01);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef((float)(1.0/53.0),(float)(1.0/10.0),100);
	glTranslatef(0.0,-10.0,-198);

	glTranslatef(0.0,20.0,145); //saghf asli masjed 
	glScalef(53.0,0.01,53.0);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef((float)(1.0/53.0),100,(float)(1.0/53.0));
	glTranslatef(0.0,-20.0,-145);
	
	glPushMatrix();
		 /// gonbad
	glBindTexture(GL_TEXTURE_2D, texture[5]);
	glClipPlane(GL_CLIP_PLANE0, eqn);
	glEnable(GL_CLIP_PLANE0);
	glTranslatef(0.0,20.0,145);
	
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluSphere(quadratic,20.0f,32,32);
	glDisable(GL_CLIP_PLANE0);
	glTranslatef(0.0,-20.0,-145);
	glPopMatrix();
	
	// soton haye divare bironi
	
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glTranslatef(65.0,12.0,27.0); //soton 45 1
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(-65.0,-12.0,-27.0);
	
	glTranslatef(72.0,12.0,40.0); //soton 45 2
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(-72.0,-12.0,-40.0);
	
	glTranslatef(72.9,12.0,60.0); //soton hayat bironi 1
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(-72.9,-12.0,-60.0);
	
	glTranslatef(72.9,12.0,75.0); //soton hayat bironi 1
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(-72.9,-12.0,-75.0);

	glTranslatef(72.9,12.0,90.0); //soton hayat bironi 1
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,1.0f,1.0f,12.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(-72.9,-12.0,-90.0);
	//*****************
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTranslatef(-5.0,10.0,100.0); //vorodi masjed chap
	glScalef(0.01,10.0,8.0);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef(100,(float)(1.0/10.0),(float)(1.0/8.0));
	glTranslatef(5.0,-10.0,-100.0);
	
	glTranslatef(5.0,10.0,100.0); //vorodi masjed rast
	glScalef(0.01,10.0,8.0);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	glDrawCube();		
	glScalef(100,(float)(1.0/10.0),(float)(1.0/8.0));
	glTranslatef(-5.0,-10.0,-100.0);

	for(int w=0;w<10;w++)
	{

		glTranslatef(-70.0+(float)(w*2.0),-9.0+(float)(w*1.0),42); //divar asli masjed samt aghab
		glScalef(1.0,0.5,47.0);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glDrawCube();		
		glScalef(1.0,2.0,(float)(1.0/47.0));
		glTranslatef(-(-70.0+((float)(w*2.0))),-(-9.0+(float)(w*1.0)),-42);
	}
	
	
	glTranslatef(12.5,29.0,-10.0); 
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	for(int q=0;q<6;q++)   //soton haye goldaste rast
	{
		glRotatef(60.0f,0.0f,1.0f,0.0f);
		glTranslatef(-0.2,0.0,1.75);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		gluCylinder(quadratic,0.1f,0.1f,4.0f,32,32);
		
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(0.2,0.0,-1.75);
	}	
	glTranslatef(-12.5,-29.0,10.0);

	glTranslatef(-12.25,29.0,-10.0); 
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	for(int q=0;q<6;q++)   //soton haye goldaste chap
	{
		glRotatef(60.0f,0.0f,1.0f,0.0f);
		glTranslatef(-0.2,0.0,1.75);
		glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
		glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
		glRotatef(90.0f,1.0f,0.0f,0.0f);
		gluCylinder(quadratic,0.1f,0.1f,4.0f,32,32);
		
		glRotatef(-90.0,1.0f,0.0f,0.0f);
		glTranslatef(0.2,0.0,-1.75);
	}	
	glTranslatef(12.5,-29.0,10.0);

	glTranslatef(12.5,30.0,-10.0);
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,2.0f,2.0f,2.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(-12.5,-30.0,10.0);
	

	glTranslatef(-12.5,30.0,-10.0);
	glRotatef(90.0f,1.0f,0.0f,0.0f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, low_shininess);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat);
	gluCylinder(quadratic,2.0f,2.0f,2.0f,32,32);
	glRotatef(-90.0,1.0f,0.0f,0.0f);
	glTranslatef(12.5,-30.0,10.0);
	
	
	return TRUE;      // Everything		Went OK]
}

	GLvoid KillGLWindow(GLvoid)								// Properly Kill The Window
	{
		if (fullscreen)										// Are We In Fullscreen Mode?
		{
			ChangeDisplaySettings(NULL,0);					// If So Switch Back To The Desktop
			ShowCursor(TRUE);								// Show Mouse Pointer
		}

		if (hRC)											// Do We Have A Rendering Context?
		{
			if (!wglMakeCurrent(NULL,NULL))					// Are We Able To Release The DC And RC Contexts?
			{
				MessageBox(NULL,"Release Of DC And RC Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
			}

			if (!wglDeleteContext(hRC))						// Are We Able To Delete The RC?
			{
				MessageBox(NULL,"Release Rendering Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
			}
			hRC=NULL;										// Set RC To NULL
		}

		if (hDC && !ReleaseDC(hWnd,hDC))					// Are We Able To Release The DC
		{
			MessageBox(NULL,"Release Device Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
			hDC=NULL;										// Set DC To NULL
		}

		if (hWnd && !DestroyWindow(hWnd))					// Are We Able To Destroy The Window?
		{
			MessageBox(NULL,"Could Not Release hWnd.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
			hWnd=NULL;										// Set hWnd To NULL
		}

		if (!UnregisterClass("OpenGL",hInstance))			// Are We Able To Unregister Class
		{
			MessageBox(NULL,"Could Not Unregister Class.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
			hInstance=NULL;									// Set hInstance To NULL
		}
	}

	/*	This Code Creates Our OpenGL Window.  Parameters Are:					*
	*	title			- Title To Appear At The Top Of The Window				*
	*	width			- Width Of The GL Window Or Fullscreen Mode				*
	*	height			- Height Of The GL Window Or Fullscreen Mode			*
	*	bits			- Number Of Bits To Use For Color (8/16/24/32)			*
	*	fullscreenflag	- Use Fullscreen Mode (TRUE) Or Windowed Mode (FALSE)	*/

	BOOL CreateGLWindow(char* title, int width, int height, int bits, bool fullscreenflag)
	{
		GLuint		PixelFormat;			// Holds The Results After Searching For A Match
		WNDCLASS	wc;						// Windows Class Structure
		DWORD		dwExStyle;				// Window Extended Style
		DWORD		dwStyle;				// Window Style
		RECT		WindowRect;				// Grabs Rectangle Upper Left / Lower Right Values
		WindowRect.left=(long)0;			// Set Left Value To 0
		WindowRect.right=(long)width;		// Set Right Value To Requested Width
		WindowRect.top=(long)0;				// Set Top Value To 0
		WindowRect.bottom=(long)height;		// Set Bottom Value To Requested Height

		fullscreen=fullscreenflag;			// Set The Global Fullscreen Flag

		hInstance			= GetModuleHandle(NULL);				// Grab An Instance For Our Window
		wc.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
		wc.lpfnWndProc		= (WNDPROC) WndProc;					// WndProc Handles Messages
		wc.cbClsExtra		= 0;									// No Extra Window Data
		wc.cbWndExtra		= 0;									// No Extra Window Data
		wc.hInstance		= hInstance;							// Set The Instance
		wc.hIcon			= LoadIcon(NULL, IDI_WINLOGO);			// Load The Default Icon
		wc.hCursor			= LoadCursor(NULL, IDC_ARROW);			// Load The Arrow Pointer
		wc.hbrBackground	= NULL;									// No Background Required For GL
		wc.lpszMenuName		= NULL;									// We Don't Want A Menu
		wc.lpszClassName	= "OpenGL";								// Set The Class Name

		if (!RegisterClass(&wc))									// Attempt To Register The Window Class
		{
			MessageBox(NULL,"Failed To Register The Window Class.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;											// Return FALSE
		}

		if (fullscreen)												// Attempt Fullscreen Mode?
		{
			DEVMODE dmScreenSettings;								// Device Mode
			memset(&dmScreenSettings,0,sizeof(dmScreenSettings));	// Makes Sure Memory's Cleared
			dmScreenSettings.dmSize=sizeof(dmScreenSettings);		// Size Of The Devmode Structure
			dmScreenSettings.dmPelsWidth	= width;				// Selected Screen Width
			dmScreenSettings.dmPelsHeight	= height;				// Selected Screen Height
			dmScreenSettings.dmBitsPerPel	= bits;					// Selected Bits Per Pixel
			dmScreenSettings.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

			// Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
			if (ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN)!=DISP_CHANGE_SUCCESSFUL)
			{
				// If The Mode Fails, Offer Two Options.  Quit Or Use Windowed Mode.
				if (MessageBox(NULL,"The Requested Fullscreen Mode Is Not Supported By\nYour Video Card. Use Windowed Mode Instead?","NeHe GL",MB_YESNO|MB_ICONEXCLAMATION)==IDYES)
				{
					fullscreen=FALSE;		// Windowed Mode Selected.  Fullscreen = FALSE
				}
				else
				{
					// Pop Up A Message Box Letting User Know The Program Is Closing.
					MessageBox(NULL,"Program Will Now Close.","ERROR",MB_OK|MB_ICONSTOP);
					return FALSE;									// Return FALSE
				}
			}
		}

		if (fullscreen)												// Are We Still In Fullscreen Mode?
		{
			dwExStyle=WS_EX_APPWINDOW;								// Window Extended Style
			dwStyle=WS_POPUP;										// Windows Style
			ShowCursor(FALSE);										// Hide Mouse Pointer
		}
		else
		{
			dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;			// Window Extended Style
			dwStyle=WS_OVERLAPPEDWINDOW;							// Windows Style
		}

		AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);		// Adjust Window To True Requested Size

		// Create The Window
		if (!(hWnd=CreateWindowEx(	dwExStyle,							// Extended Style For The Window
			"OpenGL",							// Class Name
			title,								// Window Title
			dwStyle |							// Defined Window Style
			WS_CLIPSIBLINGS |					// Required Window Style
			WS_CLIPCHILDREN,					// Required Window Style
			0, 0,								// Window Position
			WindowRect.right-WindowRect.left,	// Calculate Window Width
			WindowRect.bottom-WindowRect.top,	// Calculate Window Height
			NULL,								// No Parent Window
			NULL,								// No Menu
			hInstance,							// Instance
			NULL)))								// Dont Pass Anything To WM_CREATE
		{
			KillGLWindow();								// Reset The Display
			MessageBox(NULL,"Window Creation Error.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;								// Return FALSE
		}

		static	PIXELFORMATDESCRIPTOR pfd=				// pfd Tells Windows How We Want Things To Be
		{
			sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
			1,											// Version Number
			PFD_DRAW_TO_WINDOW |						// Format Must Support Window
			PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
			PFD_DOUBLEBUFFER,							// Must Support Double Buffering
			PFD_TYPE_RGBA,								// Request An RGBA Format
			bits,										// Select Our Color Depth
			0, 0, 0, 0, 0, 0,							// Color Bits Ignored
			0,											// No Alpha Buffer
			0,											// Shift Bit Ignored
			0,											// No Accumulation Buffer
			0, 0, 0, 0,									// Accumulation Bits Ignored
			16,											// 16Bit Z-Buffer (Depth Buffer)  
			0,											// No Stencil Buffer
			0,											// No Auxiliary Buffer
			PFD_MAIN_PLANE,								// Main Drawing Layer
			0,											// Reserved
			0, 0, 0										// Layer Masks Ignored
		};

		if (!(hDC=GetDC(hWnd)))							// Did We Get A Device Context?
		{
			KillGLWindow();								// Reset The Display
			MessageBox(NULL,"Can't Create A GL Device Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;								// Return FALSE
		}

		if (!(PixelFormat=ChoosePixelFormat(hDC,&pfd)))	// Did Windows Find A Matching Pixel Format?
		{
			KillGLWindow();								// Reset The Display
			MessageBox(NULL,"Can't Find A Suitable PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;								// Return FALSE
		}

		if(!SetPixelFormat(hDC,PixelFormat,&pfd))		// Are We Able To Set The Pixel Format?
		{
			KillGLWindow();								// Reset The Display
			MessageBox(NULL,"Can't Set The PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;								// Return FALSE
		}

		if (!(hRC=wglCreateContext(hDC)))				// Are We Able To Get A Rendering Context?
		{
			KillGLWindow();								// Reset The Display
			MessageBox(NULL,"Can't Create A GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;								// Return FALSE
		}

		if(!wglMakeCurrent(hDC,hRC))					// Try To Activate The Rendering Context
		{
			KillGLWindow();								// Reset The Display
			MessageBox(NULL,"Can't Activate The GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;								// Return FALSE
		}

		ShowWindow(hWnd,SW_SHOW);						// Show The Window
		SetForegroundWindow(hWnd);						// Slightly Higher Priority
		SetFocus(hWnd);									// Sets Keyboard Focus To The Window
		ReSizeGLScene(width, height);					// Set Up Our Perspective GL Screen

		if (!InitGL())									// Initialize Our Newly Created GL Window
		{
			KillGLWindow();								// Reset The Display
			MessageBox(NULL,"Initialization Failed.","ERROR",MB_OK|MB_ICONEXCLAMATION);
			return FALSE;								// Return FALSE
		}

		return TRUE;									// Success
	}

	LRESULT CALLBACK WndProc(	HWND	hWnd,			// Handle For This Window
		UINT	uMsg,			// Message For This Window
		WPARAM	wParam,			// Additional Message Information
		LPARAM	lParam)			// Additional Message Information
	{
		switch (uMsg)									// Check For Windows Messages
		{
		case WM_ACTIVATE:							// Watch For Window Activate Message
			{
				if (!HIWORD(wParam))					// Check Minimization State
				{
					active=TRUE;						// Program Is Active
				}
				else
				{
					active=FALSE;						// Program Is No Longer Active
				}

				return 0;								// Return To The Message Loop
			}

		case WM_SYSCOMMAND:							// Intercept System Commands
			{
				switch (wParam)							// Check System Calls
				{
				case SC_SCREENSAVE:					// Screensaver Trying To Start?
				case SC_MONITORPOWER:				// Monitor Trying To Enter Powersave?
					return 0;							// Prevent From Happening
				}
				break;									// Exit
			}

		case WM_CLOSE:								// Did We Receive A Close Message?
			{
				PostQuitMessage(0);						// Send A Quit Message
				return 0;								// Jump Back
			}

		case WM_KEYDOWN:							// Is A Key Being Held Down?
			{
				keys[wParam] = TRUE;					// If So, Mark It As TRUE
				return 0;								// Jump Back
			}

		case WM_KEYUP:								// Has A Key Been Released?
			{
				keys[wParam] = FALSE;					// If So, Mark It As FALSE
				return 0;								// Jump Back
			}

		case WM_SIZE:								// Resize The OpenGL Window
			{
				ReSizeGLScene(LOWORD(lParam),HIWORD(lParam));  // LoWord=Width, HiWord=Height
				return 0;								// Jump Back
			}
		}

		// Pass All Unhandled Messages To DefWindowProc
		return DefWindowProc(hWnd,uMsg,wParam,lParam);
	}

	int WINAPI WinMain(	HINSTANCE	hInstance,			// Instance
		HINSTANCE	hPrevInstance,		// Previous Instance
		LPSTR		lpCmdLine,			// Command Line Parameters
		int			nCmdShow)			// Window Show State
	{
		MSG		msg;									// Windows Message Structure
		BOOL	done=FALSE;								// Bool Variable To Exit Loop

		// Ask The User Which Screen Mode They Prefer
		if (MessageBox(NULL,"Would You Like To Run In Fullscreen Mode?", "Start FullScreen?",MB_YESNO|MB_ICONQUESTION)==IDNO)
		{
			fullscreen=FALSE;							// Windowed Mode
		}

		// Create Our OpenGL Window
		if (!CreateGLWindow("FINAL",640,480,16,fullscreen))
		{
			return 0;									// Quit If Window Was Not Created
		}

		while(!done)									// Loop That Runs While done=FALSE
		{
			if (PeekMessage(&msg,NULL,0,0,PM_REMOVE))	// Is There A Message Waiting?
			{
				if (msg.message==WM_QUIT)				// Have We Received A Quit Message?
				{
					done=TRUE;							// If So done=TRUE
				}
				else									// If Not, Deal With Window Messages
				{
					TranslateMessage(&msg);				// Translate The Message
					DispatchMessage(&msg);				// Dispatch The Message
				}
			}
			else										// If There Are No Messages
			{
				// Draw The Scene.  Watch For ESC Key And Quit Messages From DrawGLScene()
				if ((active && !DrawGLScene()) || keys[VK_ESCAPE])	// Active?  Was There A Quit Received?
				{
					done=TRUE;							// ESC or DrawGLScene Signalled A Quit
				}
				else									// Not Time To Quit, Update Screen
				{
					SwapBuffers(hDC);					// Swap Buffers (Double Buffering)
					if (keys['B'] && !bp)
					{
						bp=TRUE;
						blend=!blend;
						if (!blend)
						{
							glDisable(GL_BLEND);
							glEnable(GL_DEPTH_TEST);
						}
						else
						{
							glEnable(GL_BLEND);
							glDisable(GL_DEPTH_TEST);
						}
					}
					if (!keys['B'])
					{
						bp=FALSE;
					}

					if (keys['F'] && !fp)
					{
						fp=TRUE;
						filter+=1;
						if (filter>2)
						{
							filter=0;
						}
					}
					if (!keys['F'])
					{
						fp=FALSE;
					}
					if (keys['U'])
					{
						ytemp+=up_down_speed;
					}
					if (keys['D'])
					{
						cy+=0.0025;
						ytemp-=up_down_speed;
					}

					if (keys[VK_PRIOR])
					{
						z-=0.02f;
					}

					if (keys[VK_NEXT])
					{
						z+=0.02f;
					}

					if (keys[VK_UP])                                                
					{
						//	moveMeFlat(1);
						bool flag = false;
						float xtemp=0,ztemp=0;
						xpos -= (float)sin(heading*piover180) * 0.5f;
						zpos-= (float)cos(heading*piover180) * 0.5f;
					/*	for(int y=0;y<numtriangles;y++)
						{
							if(((world[y].A*xtemp)+(world[y].B*ytemp )+(world[y].C*ztemp))== world[y].D)
							{
								flag= true;
								break;
							}
						}
						if(!flag)
						{
							xpos=xtemp;
							zpos=ztemp;

						}
						else
							flag=false;
						*/if (walkbiasangle >= 359.0f)
						{
							walkbiasangle = 0.0f;
						}
						else
						{
							walkbiasangle+= 3;
						}
						walkbias = (float)sin(walkbiasangle * piover180)/20.0f;
					}

					if (keys[VK_DOWN])
					{
						//	moveMeFlat(-1);
						xpos += (float)sin(heading*piover180) * 0.5f;
						zpos += (float)cos(heading*piover180) * 0.5f;
						if (walkbiasangle <= 1.0f)
						{
							walkbiasangle = 359.0f;
						}
						else
						{
							walkbiasangle-= 3;
						}
						walkbias = (float)sin(walkbiasangle * piover180)/20.0f;
					}

					if (keys[VK_RIGHT])
					{
						//	angle += 0.0001f; ///
						//	orientMe(angle);///
						heading -= 1.0;//0.5;///1.0f;
						yrot = heading;
					}

					if (keys[VK_LEFT])
					{
						//	angle -= 0.0001f; /////
						//	orientMe(angle); /////
						heading += 1.0;//0.5;//1.0f;	
						yrot = heading;
					}

					if (keys[VK_PRIOR])
					{
						lookupdown-= 1.0f;

					}

					if (keys[VK_NEXT])
					{
						lookupdown+= 1.0f;
					}

					if (keys[VK_F1])						// Is F1 Being Pressed?
					{
						keys[VK_F1]=FALSE;					// If So Make Key FALSE
						KillGLWindow();						// Kill Our Current Window
						fullscreen=!fullscreen;				// Toggle Fullscreen / Windowed Mode
						// Recreate Our OpenGL Window
						if (!CreateGLWindow("FIANL",640,480,16,fullscreen))
						{
							return 0;						// Quit If Window Was Not Created
						}
					}
				}
			}
		}

		// Shutdown
		KillGLWindow();										// Kill The Window
		return (msg.wParam);								// Exit The Program
	}

