/* 
 * Copyright (c) 2018 NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//-----------------------------------------------------------------------------
//
// optixPathTracer: simple interactive path tracer
//
//-----------------------------------------------------------------------------

#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glew.h>
#  if defined( _WIN32 )
#    include <GL/wglew.h>
#    include <GL/freeglut.h>
#  else
#    include <GL/glut.h>
#  endif
#endif

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_stream_namespace.h>

#include "optixPathTracer.h"
#include <sutil.h>
#include <Arcball.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdint.h>
#include <cstdlib>

#include "buffers.h"
#include "binaryio.h"

using namespace optix;

const char* const SAMPLE_NAME = "DasRender";

//------------------------------------------------------------------------------
//
// Globals
//
//------------------------------------------------------------------------------

Context        context = 0;
uint32_t       width  = 512;
uint32_t       height = 512;
bool           use_pbo = true;

int            frame_number = 1;
int            rr_begin_bounce = 1;
int            rr_max_bounce = 10;
Program        pgram_intersection = 0;
Program        pgram_bounding_box = 0;
Program        sphere_intersection = 0;
Program        sphere_bounding_box = 0;

// Camera state
float3         camera_up;
float3         camera_lookat;
float3         camera_eye;
Matrix4x4      camera_rotate;
bool           camera_changed = true;
sutil::Arcball arcball;

// Mouse state
int2           mouse_prev_pos;
int            mouse_button;


//------------------------------------------------------------------------------
//
// Forward decls 
//
//------------------------------------------------------------------------------

Buffer getOutputBuffer();
Buffers & getOutputsBuffers();
void destroyContext();
void registerExitHandler();
void createContext(const int sample_num);
void loadGeometry();
void setupCamera();
void updateCamera();
void glutInitialize( int* argc, char** argv );
void glutRun();

void glutDisplay();
void glutKeyboardPress( unsigned char k, int x, int y );
void glutMousePress( int button, int state, int x, int y );
void glutMouseMotion( int x, int y);
void glutResize( int w, int h );


//------------------------------------------------------------------------------
//
//  Helper functions
//
//------------------------------------------------------------------------------

Buffer getOutputBuffer()
{
    return context[ "output_buffer" ]->getBuffer();
}

Buffers & getOutputsBuffers()
{
	Buffers buffers;
	buffers.output = context["output_buffer"]->getBuffer();
	buffers.depth = context["depth_buffer"]->getBuffer();
	buffers.texture = context["texture_buffer"]->getBuffer();
	buffers.normal = context["normal_buffer"]->getBuffer();
	buffers.shadow = context["shadow_buffer"]->getBuffer();
	return buffers;
}


void destroyContext()
{
    if( context )
    {
        context->destroy();
        context = 0;
    }
}


void registerExitHandler()
{
    // register shutdown handler
#ifdef _WIN32
    glutCloseFunc( destroyContext );  // this function is freeglut-only
#else
    atexit( destroyContext );
#endif
}


void setMaterial(
        GeometryInstance& gi,
        Material material,
        const std::string& color_name,
        const float3& color)
{
    gi->addMaterial(material);
    gi[color_name]->setFloat(color);
}


GeometryInstance createParallelogram(
        const float3& anchor,
        const float3& offset1,
        const float3& offset2)
{
    Geometry parallelogram = context->createGeometry();
    parallelogram->setPrimitiveCount( 1u );
    parallelogram->setIntersectionProgram( pgram_intersection );
    parallelogram->setBoundingBoxProgram( pgram_bounding_box );

    float3 normal = normalize( cross( offset1, offset2 ) );
    float d = dot( normal, anchor );
    float4 plane = make_float4( normal, d );

    float3 v1 = offset1 / dot( offset1, offset1 );
    float3 v2 = offset2 / dot( offset2, offset2 );

    parallelogram["plane"]->setFloat( plane );
    parallelogram["anchor"]->setFloat( anchor );
    parallelogram["v1"]->setFloat( v1 );
    parallelogram["v2"]->setFloat( v2 );

    GeometryInstance gi = context->createGeometryInstance();
    gi->setGeometry(parallelogram);
    return gi;
}

GeometryInstance createSphere(const float3& origin, const float& radius)
{
	Geometry sphere = context->createGeometry();
	sphere->setPrimitiveCount( 1u );
	sphere->setIntersectionProgram(sphere_intersection);
	sphere->setBoundingBoxProgram(sphere_bounding_box);

	float4 sphere_loc = make_float4(origin, radius);
	sphere["sphere"]->setFloat(sphere_loc);

	GeometryInstance gi = context->createGeometryInstance();
	gi->setGeometry(sphere);
	return gi;
}


void createContext(const int num_samples)
{
    context = Context::create();
    context->setRayTypeCount( 2 );
    context->setEntryPointCount( 1 );
    context->setStackSize( 1800 );

    context[ "scene_epsilon"                  ]->setFloat( 1.e-3f );
    context[ "pathtrace_ray_type"             ]->setUint( 0u );
    context[ "pathtrace_shadow_ray_type"      ]->setUint( 1u );
    context[ "rr_begin_bounce"                 ]->setUint( rr_begin_bounce );
    context[ "rr_max_bounce"                 ]->setUint( rr_max_bounce );

	Buffer output = sutil::createOutputBuffer(context, RT_FORMAT_FLOAT4, width, height, use_pbo);
	context["output_buffer"]->set(output);
	Buffer depth = sutil::createOutputBuffer(context, RT_FORMAT_FLOAT4, width, height, use_pbo);
	context["depth_buffer"]->set(depth);
	Buffer texture = sutil::createOutputBuffer(context, RT_FORMAT_FLOAT4, width, height, use_pbo);
	context["texture_buffer"]->set(texture);
	Buffer normal = sutil::createOutputBuffer(context, RT_FORMAT_FLOAT4, width, height, use_pbo);
	context["normal_buffer"]->set(normal);
	Buffer shadow = sutil::createOutputBuffer(context, RT_FORMAT_FLOAT4, width, height, use_pbo);
	context["shadow_buffer"]->set(shadow);

    // Setup programs
    const char *ptx = sutil::getPtxString( SAMPLE_NAME, "optixPathTracer.cu" );
    context->setRayGenerationProgram( 0, context->createProgramFromPTXString( ptx, "pathtrace_camera" ) );
    context->setExceptionProgram( 0, context->createProgramFromPTXString( ptx, "exception" ) );
    context->setMissProgram( 0, context->createProgramFromPTXString( ptx, "miss" ) );

    context[ "num_samples" ]->setUint(num_samples);
    context[ "bad_color"        ]->setFloat( 1000000.0f, 0.0f, 1000000.0f ); // Super magenta to make sure it doesn't get averaged out in the progressive rendering.
    context[ "bg_color"         ]->setFloat( make_float3(0.0f) );
}


void loadGeometry()
{
    // Light buffer
    ParallelogramLight light;
    light.corner   = make_float3( 343.0f, 548.6f, 227.0f);
    light.v1       = make_float3( -130.0f, 0.0f, 0.0f);
    light.v2       = make_float3( 0.0f, 0.0f, 105.0f);
    light.normal   = normalize( cross(light.v1, light.v2) );
    light.emission = make_float3( 15.0f, 15.0f, 5.0f );

    Buffer light_buffer = context->createBuffer( RT_BUFFER_INPUT );
    light_buffer->setFormat( RT_FORMAT_USER );
    light_buffer->setElementSize( sizeof( ParallelogramLight ) );
    light_buffer->setSize( 1u );
    memcpy( light_buffer->map(), &light, sizeof( light ) );
    light_buffer->unmap();
    context["lights"]->setBuffer( light_buffer );


    // Set up material
    Material diffuse = context->createMaterial();
    const char *ptx = sutil::getPtxString( SAMPLE_NAME, "optixPathTracer.cu" );
    Program diffuse_ch = context->createProgramFromPTXString( ptx, "diffuse" );
    Program diffuse_ah = context->createProgramFromPTXString( ptx, "shadow" );
    diffuse->setClosestHitProgram( 0, diffuse_ch );
    diffuse->setAnyHitProgram( 1, diffuse_ah );

    Material diffuse_light = context->createMaterial();
    Program diffuse_em = context->createProgramFromPTXString( ptx, "diffuseEmitter" );
    diffuse_light->setClosestHitProgram( 0, diffuse_em );

	Material specular = context->createMaterial();
	Program specular_ch = context->createProgramFromPTXString(ptx, "specular_closest_hit_radiance");
	specular->setClosestHitProgram(0, specular_ch);
	specular->setAnyHitProgram(1, diffuse_ah);

	Material glass = context->createMaterial();
	Program glass_ch = context->createProgramFromPTXString(ptx, "glass_closest_hit_radiance");
	glass->setClosestHitProgram(0, glass_ch);
	glass->setAnyHitProgram(1, diffuse_ah);
	glass["cutoff_color"]->setFloat(0.34f, 0.55f, 0.85f);
	glass["fresnel_exponent"]->setFloat(3.0f);
	glass["fresnel_minimum"]->setFloat(0.1f);
	glass["fresnel_maximum"]->setFloat(1.0f);
	glass["refraction_index"]->setFloat(1.6f);
	glass["refraction_color"]->setFloat(1.0f, 1.0f, 1.0f);
	glass["reflection_color"]->setFloat(1.0f, 1.0f, 1.0f);
	glass["refraction_maxdepth"]->setInt(10);
	glass["reflection_maxdepth"]->setInt(10);
	float3 extinction = make_float3(.80f, .89f, .75f);
	glass["extinction_constant"]->setFloat(log(extinction.x), log(extinction.y), log(extinction.z));

    // Set up parallelogram programs
    ptx = sutil::getPtxString( SAMPLE_NAME, "parallelogram.cu" );
    pgram_bounding_box = context->createProgramFromPTXString( ptx, "bounds" );
    pgram_intersection = context->createProgramFromPTXString( ptx, "intersect" );

	// Set up sphere programs
	ptx = sutil::getPtxString(SAMPLE_NAME, "sphere.cu");
	sphere_bounding_box = context->createProgramFromPTXString(ptx, "bounds");
	sphere_intersection = context->createProgramFromPTXString(ptx, "intersect");

    // create geometry instances
    std::vector<GeometryInstance> gis;

    const float3 white = make_float3( 0.8f, 0.8f, 0.8f );
    const float3 green = make_float3( 0.05f, 0.8f, 0.05f );
    const float3 red   = make_float3( 0.8f, 0.05f, 0.05f );
    const float3 light_em = make_float3( 15.0f, 15.0f, 5.0f );

    // Floor
    gis.push_back( createParallelogram( make_float3( 0.0f, 0.0f, 0.0f ),
                                        make_float3( 0.0f, 0.0f, 559.2f ),
                                        make_float3( 556.0f, 0.0f, 0.0f ) ) );
    setMaterial(gis.back(), diffuse, "diffuse_color", white);

    // Ceiling
    gis.push_back( createParallelogram( make_float3( 0.0f, 548.8f, 0.0f ),
                                        make_float3( 556.0f, 0.0f, 0.0f ),
                                        make_float3( 0.0f, 0.0f, 559.2f ) ) );
    setMaterial(gis.back(), diffuse, "diffuse_color", white);

    // Back wall
    gis.push_back( createParallelogram( make_float3( 0.0f, 0.0f, 559.2f),
                                        make_float3( 0.0f, 548.8f, 0.0f),
                                        make_float3( 556.0f, 0.0f, 0.0f) ) );
    setMaterial(gis.back(), diffuse, "diffuse_color", white);

    // Right wall
    gis.push_back( createParallelogram( make_float3( 0.0f, 0.0f, 0.0f ),
                                        make_float3( 0.0f, 548.8f, 0.0f ),
                                        make_float3( 0.0f, 0.0f, 559.2f ) ) );
    setMaterial(gis.back(), diffuse, "diffuse_color", green);

    // Left wall
    gis.push_back( createParallelogram( make_float3( 556.0f, 0.0f, 0.0f ),
                                        make_float3( 0.0f, 0.0f, 559.2f ),
                                        make_float3( 0.0f, 548.8f, 0.0f ) ) );
    setMaterial(gis.back(), diffuse, "diffuse_color", red);

	gis.push_back(createSphere(make_float3(128.0f, 100.0f, 250.0f), 100.0f));
	gis.back()->addMaterial(glass);
	//setMaterial(gis.back(), diffuse, "diffuse_color", red);

    // Short block
 //   gis.push_back( createParallelogram( make_float3( 130.0f, 455.0f, 65.0f),
 //                                       make_float3( -48.0f, 0.0f, 160.0f),
 //                                       make_float3( 160.0f, 0.0f, 49.0f) ) );
 //   setMaterial(gis.back(), glass, "diffuse_color", white);
 //   gis.push_back( createParallelogram( make_float3( 290.0f, 290.0f, 114.0f),
 //                                       make_float3( 0.0f, 165.0f, 0.0f),
 //                                       make_float3( -50.0f, 0.0f, 158.0f) ) );
 //   setMaterial(gis.back(), glass, "diffuse_color", white);
 //   gis.push_back( createParallelogram( make_float3( 130.0f, 290.0f, 65.0f),
 //                                       make_float3( 0.0f, 165.0f, 0.0f),
 //                                       make_float3( 160.0f, 0.0f, 49.0f) ) );
 //   setMaterial(gis.back(), glass, "diffuse_color", white);
 //   gis.push_back( createParallelogram( make_float3( 82.0f, 290.0f, 225.0f),
 //                                       make_float3( 0.0f, 165.0f, 0.0f),
 //                                       make_float3( 48.0f, 0.0f, -160.0f) ) );
 //   setMaterial(gis.back(), glass, "diffuse_color", white);
 //   gis.push_back( createParallelogram( make_float3( 240.0f, 290.0f, 272.0f),
 //                                       make_float3( 0.0f, 165.0f, 0.0f),
 //                                       make_float3( -158.0f, 0.0f, -47.0f) ) );
 //   setMaterial(gis.back(), glass, "diffuse_color", white);
	//gis.push_back(createParallelogram(  make_float3(130.0f, 290.0f, 65.0f),
	//								    make_float3(-48.0f, 0.0f, 160.0f),
	//								    make_float3(160.0f, 0.0f, 49.0f)));
	//setMaterial(gis.back(), glass, "diffuse_color", white);

    //// Tall block
    //gis.push_back( createParallelogram( make_float3( 423.0f, 330.0f, 247.0f),
    //                                    make_float3( -158.0f, 0.0f, 49.0f),
    //                                    make_float3( 49.0f, 0.0f, 159.0f) ) );
    //setMaterial(gis.back(), diffuse, "diffuse_color", white);
    //gis.push_back( createParallelogram( make_float3( 423.0f, 0.0f, 247.0f),
    //                                    make_float3( 0.0f, 330.0f, 0.0f),
    //                                    make_float3( 49.0f, 0.0f, 159.0f) ) );
    //setMaterial(gis.back(), diffuse, "diffuse_color", white);
    //gis.push_back( createParallelogram( make_float3( 472.0f, 0.0f, 406.0f),
    //                                    make_float3( 0.0f, 330.0f, 0.0f),
    //                                    make_float3( -158.0f, 0.0f, 50.0f) ) );
    //setMaterial(gis.back(), diffuse, "diffuse_color", white);
    //gis.push_back( createParallelogram( make_float3( 314.0f, 0.0f, 456.0f),
    //                                    make_float3( 0.0f, 330.0f, 0.0f),
    //                                    make_float3( -49.0f, 0.0f, -160.0f) ) );
    //setMaterial(gis.back(), diffuse, "diffuse_color", white);
    //gis.push_back( createParallelogram( make_float3( 265.0f, 0.0f, 296.0f),
    //                                    make_float3( 0.0f, 330.0f, 0.0f),
    //                                    make_float3( 158.0f, 0.0f, -49.0f) ) );
    //setMaterial(gis.back(), diffuse, "diffuse_color", white);

    // Create shadow group (no light)
    GeometryGroup shadow_group = context->createGeometryGroup(gis.begin(), gis.end());
    shadow_group->setAcceleration( context->createAcceleration( "Trbvh" ) );
    context["top_shadower"]->set( shadow_group );

    // Light
    gis.push_back( createParallelogram( make_float3( 343.0f, 548.6f, 227.0f),
                                        make_float3( -130.0f, 0.0f, 0.0f),
                                        make_float3( 0.0f, 0.0f, 105.0f) ) );
    setMaterial(gis.back(), diffuse_light, "emission_color", light_em);

    // Create geometry group
    GeometryGroup geometry_group = context->createGeometryGroup(gis.begin(), gis.end());
    geometry_group->setAcceleration( context->createAcceleration( "Trbvh" ) );
    context["top_object"]->set( geometry_group );
}

  
void setupCamera()
{
    camera_eye    = make_float3( 278.0f, 273.0f, -900.0f );
    camera_lookat = make_float3( 278.0f, 273.0f,    0.0f );
    camera_up     = make_float3(   0.0f,   1.0f,    0.0f );

    camera_rotate  = Matrix4x4::identity();
}


void updateCamera()
{
    const float fov  = 35.0f;
    const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
    
    float3 camera_u, camera_v, camera_w;
    sutil::calculateCameraVariables(
            camera_eye, camera_lookat, camera_up, fov, aspect_ratio,
            camera_u, camera_v, camera_w, /*fov_is_vertical*/ true );

    const Matrix4x4 frame = Matrix4x4::fromBasis( 
            normalize( camera_u ),
            normalize( camera_v ),
            normalize( -camera_w ),
            camera_lookat);
    const Matrix4x4 frame_inv = frame.inverse();
    // Apply camera rotation twice to match old SDK behavior
    const Matrix4x4 trans     = frame*camera_rotate*camera_rotate*frame_inv; 

    camera_eye    = make_float3( trans*make_float4( camera_eye,    1.0f ) );
    camera_lookat = make_float3( trans*make_float4( camera_lookat, 1.0f ) );
    camera_up     = make_float3( trans*make_float4( camera_up,     0.0f ) );

    sutil::calculateCameraVariables(
            camera_eye, camera_lookat, camera_up, fov, aspect_ratio,
            camera_u, camera_v, camera_w, true );

    camera_rotate = Matrix4x4::identity();

    if( camera_changed ) // reset accumulation
        frame_number = 1;
    camera_changed = false;

    context[ "frame_number" ]->setUint( frame_number++ );
    context[ "eye"]->setFloat( camera_eye );
    context[ "U"  ]->setFloat( camera_u );
    context[ "V"  ]->setFloat( camera_v );
    context[ "W"  ]->setFloat( camera_w );

}


void glutInitialize( int* argc, char** argv )
{
    glutInit( argc, argv );
    glutInitDisplayMode( GLUT_RGB | GLUT_ALPHA | GLUT_DEPTH | GLUT_DOUBLE );
    glutInitWindowSize( width, height );
    glutInitWindowPosition( 100, 100 );                                               
    glutCreateWindow( SAMPLE_NAME );
    glutHideWindow();                                                              
}


void glutRun()
{
    // Initialize GL state                                                            
    glMatrixMode(GL_PROJECTION);                                                   
    glLoadIdentity();                                                              
    glOrtho(0, 1, 0, 1, -1, 1 );                                                   

    glMatrixMode(GL_MODELVIEW);                                                    
    glLoadIdentity();                                                              

    glViewport(0, 0, width, height);                                 

    glutShowWindow();                                                              
    glutReshapeWindow( width, height);

    // register glut callbacks
    glutDisplayFunc( glutDisplay );
    glutIdleFunc( glutDisplay );
    glutReshapeFunc( glutResize );
    glutKeyboardFunc( glutKeyboardPress );
    glutMouseFunc( glutMousePress );
    glutMotionFunc( glutMouseMotion );

    registerExitHandler();

    glutMainLoop();
}


//------------------------------------------------------------------------------
//
//  GLUT callbacks
//
//------------------------------------------------------------------------------

void glutDisplay()
{
    updateCamera();
    context->launch( 0, width, height );

    sutil::displayBufferGL( getOutputBuffer() );

    {
      static unsigned frame_count = 0;
      sutil::displayFps( frame_count++ );
    }

    glutSwapBuffers();
}


void glutKeyboardPress( unsigned char k, int x, int y )
{

    switch( k )
    {
        case( 'q' ):
        case( 27 ): // ESC
        {
            destroyContext();
            exit(0);
        }
        case( 's' ):
        {
            const std::string outputImage = std::string(SAMPLE_NAME) + ".ppm";
            std::cerr << "Saving current frame to '" << outputImage << "'\n";
            sutil::displayBufferPPM( outputImage.c_str(), getOutputBuffer(), false );
            break;
        }
    }
}


void glutMousePress( int button, int state, int x, int y )
{
    if( state == GLUT_DOWN )
    {
        mouse_button = button;
        mouse_prev_pos = make_int2( x, y );
    }
    else
    {
        // nothing
    }
}


void glutMouseMotion( int x, int y)
{
    if( mouse_button == GLUT_RIGHT_BUTTON )
    {
        const float dx = static_cast<float>( x - mouse_prev_pos.x ) /
                         static_cast<float>( width );
        const float dy = static_cast<float>( y - mouse_prev_pos.y ) /
                         static_cast<float>( height );
        const float dmax = fabsf( dx ) > fabs( dy ) ? dx : dy;
        const float scale = std::min<float>( dmax, 0.9f );
        camera_eye = camera_eye + (camera_lookat - camera_eye)*scale;
        camera_changed = true;
    }
    else if( mouse_button == GLUT_LEFT_BUTTON )
    {
        const float2 from = { static_cast<float>(mouse_prev_pos.x),
                              static_cast<float>(mouse_prev_pos.y) };
        const float2 to   = { static_cast<float>(x),
                              static_cast<float>(y) };

        const float2 a = { from.x / width, from.y / height };
        const float2 b = { to.x   / width, to.y   / height };

        camera_rotate = arcball.rotate( b, a );
        camera_changed = true;
    }

    mouse_prev_pos = make_int2( x, y );
}


void glutResize( int w, int h )
{
    if ( w == (int)width && h == (int)height ) return;

    camera_changed = true;

    width  = w;
    height = h;
    sutil::ensureMinimumSize(width, height);

    sutil::resizeBuffer( getOutputBuffer(), width, height );

    glViewport(0, 0, width, height);                                               

    glutPostRedisplay();
}


//------------------------------------------------------------------------------
//
// Main
//
//------------------------------------------------------------------------------

void printUsageAndExit( const std::string& argv0 )
{
    std::cerr << "\nUsage: " << argv0 << " [options]\n";
    std::cerr <<
        "App Options:\n"
        "  -h | --help               Print this usage message and exit.\n"
        "  -f | --file               Save single frame to file and exit.\n"
		"  -n | --nopbo              Disable GL interop for display buffer.\n"
		"  -s | --spp                Sample per pixel num,.\n"
		"  -i | --id				 index for output name,.\n"
        "App Keystrokes:\n"
        "  q  Quit\n" 
        "  s  Save image to '" << SAMPLE_NAME << ".ppm'\n"
        << std::endl;

    exit(1);
}


int main( int argc, char** argv )
 {
    std::string out_file;
	int n_samples = 4;
	int id = 0;

    for( int i=1; i<argc; ++i )
    {
        const std::string arg( argv[i] );

        if( arg == "-h" || arg == "--help" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "-f" || arg == "--file"  )
        {
            //if( i == argc-1 )
            //{
            //    std::cerr << "Option '" << arg << "' requires additional argument.\n";
            //    printUsageAndExit( argv[0] );
            //}
            out_file = argv[++i];
        }
        else if( arg == "-n" || arg == "--nopbo"  )
        {
            use_pbo = false;
        }
		else if (arg == "-s" || arg == "--spp")
		{
			if (atoi(argv[++i]) != 0) {
				n_samples = atoi(argv[i]);
			}
			else {
				std::cerr << "Option -s or --spp'" << arg << "' requires positive integer.\n";
				printUsageAndExit(argv[0]);
			}
		}
		else if (arg == "-i" || arg == "--id") {
			if (atoi(argv[++i]) >= 0) {
				id = atoi(argv[i]);
			}
			else {
				std::cerr << "Option -i or --id'" << arg << "' requires positive integer.\n";
				printUsageAndExit(argv[0]);
			}
		}
        else
        {
            std::cerr << "Unknown option '" << arg << "'\n";
            printUsageAndExit( argv[0] );
        }
    }

		try
		{
			glutInitialize(&argc, argv);

#ifndef __APPLE__
			glewInit();
#endif

			createContext(n_samples);
			setupCamera();
			loadGeometry();

			context->validate();

			if (out_file.empty())
			{
				glutRun();
			}
			else
			{
				updateCamera();
				context->launch(0, width, height);
				Buffers buffers = getOutputsBuffers();
				buffers.displayBuffers(out_file);
				buffers.saveBins(out_file);
				destroyContext();
			}

			return 0;
		}
		SUTIL_CATCH(context->get())
}

