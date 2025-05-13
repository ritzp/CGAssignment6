//
//  sphere_scene.c
//  Rasterizer
//
//

#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>
#include <limits>

using namespace std;

const int WIDTH = 512;
const int HEIGHT = 512;

struct Vertex {
public:
    float x, y, z, w = 1.0f;
};

struct Vector3 {
public:
    float x, y, z;

    Vector3 operator+(const Vector3& rhs) const { return { x + rhs.x, y + rhs.y, z + rhs.z }; }
    Vector3 operator-(const Vector3& rhs) const { return { x - rhs.x, y - rhs.y, z - rhs.z }; }
    Vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vector3 operator*(const Vector3& s) const { return { x * s.x, y * s.y, z * s.z }; }
    Vector3 operator/(float s) const { return { x / s, y / s, z / s }; }
};

int gNumVertices;
int gNumTriangles;
std::vector<Vertex> gVertices;
int* gIndexBuffer;
float depthBuffer[WIDTH][HEIGHT];
std::vector<float> OutputImage;

// Matrix Multiplication Helper
Vertex multiply(const float matrix[4][4], const Vertex& v) {
    Vertex result = { 0, 0, 0, 0 };
    result.x = matrix[0][0] * v.x + matrix[0][1] * v.y + matrix[0][2] * v.z + matrix[0][3] * v.w;
    result.y = matrix[1][0] * v.x + matrix[1][1] * v.y + matrix[1][2] * v.z + matrix[1][3] * v.w;
    result.z = matrix[2][0] * v.x + matrix[2][1] * v.y + matrix[2][2] * v.z + matrix[2][3] * v.w;
    result.w = matrix[3][0] * v.x + matrix[3][1] * v.y + matrix[3][2] * v.z + matrix[3][3] * v.w;
    return result;
}

Vertex modeling_transform(Vertex _v, float sx, float sy, float sz, float tx, float ty, float tz) {
    float mm[4][4] = {
        {sx, 0, 0, tx},
        {0, sy, 0, ty},
        {0, 0, sz, tz},
        { 0, 0, 0, 1 }
    };
    return multiply(mm, _v);
}

Vertex camera_transform(Vertex _v, Vector3 u, Vector3 v, Vector3 w, Vector3 e) {
    float mc[4][4] = {
        {u.x, v.x, w.x, e.x},
        {u.y, v.y, w.y, e.y},
        {u.z, v.z, w.z, e.z},
        {0, 0, 0, 1}
    };
    return multiply(mc, _v);
}

Vertex projection_transform(Vertex _v, float l, float r, float t, float b, float n, float f) {
    float mp[4][4] = {
        {2 * n / (r - l), 0, (l + r) / (l - r), 0},
        {0, 2 * n / (t - b), (b + t) / (b - t), 0},
        {0, 0, (f + n) / (n - f), (2 * f * n) / (f - n)},
        {0, 0, 1, 0}
    };
    _v = multiply(mp, _v);
    if (_v.w != 0) {
        _v.x /= _v.w;
        _v.y /= _v.w;
        _v.z /= _v.w;
        _v.w = 1.0f;
    }
    return _v;
}

Vertex viewport_transform(Vertex v, float width, float height) {
    v.x = (v.x + 1) * width * 0.5f;
    v.y = (v.y + 1) * height * 0.5f;
    return v;
}

std::vector<Vertex> transformedVertices;
std::vector<Vector3> posWorlds;

void transform_vertices() {
    transformedVertices.resize(gVertices.size());
    posWorlds.resize(gVertices.size());

    Vector3 u = { 1, 0, 0 };
    Vector3 v = { 0, 1, 0 };
    Vector3 w = { 0, 0, 1 };
    Vector3 e = { 0, 0, 0 };

    for (size_t i = 0; i < gVertices.size(); ++i) {
        Vertex vert = gVertices[i];

        vert = modeling_transform(vert, 2, 2, 2, 0, 0, -7);
        posWorlds[i] = { vert.x, vert.y, vert.z };

        vert = camera_transform(vert, u, v, w, e);
        vert = projection_transform(vert, -0.1, 0.1, 0.1, -0.1, -0.1, -1000);
        vert = viewport_transform(vert, WIDTH, HEIGHT);

        transformedVertices[i] = vert;
    }
}

Vector3 normalize(const Vector3& v) {
    float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0f)
        return v / len;
    return { 0.0f, 0.0f, 0.0f };
}

float dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vector3 shading(Vector3 normal, Vector3 lightPos, Vector3 viewPos, Vector3 pos) {
    Vector3 n = normalize(normal);
    Vector3 l = normalize(lightPos - pos);
    Vector3 v = normalize(viewPos - pos);
    Vector3 h = normalize(l + v);

    Vector3 ka = { 0.0f, 1.0f, 0.0f };
    Vector3 kd = { 0.0f, 0.5f, 0.0f };
    Vector3 ks = { 0.5f, 0.5f, 0.5f };
    float p = 32.0f;

    float Ia = 0.2f;
    float Id = 1.0f;

    Vector3 ambient = ka * Ia;
    Vector3 diffuse = kd * Id * max(0.0f, dot(n, l));
    Vector3 specular = ks * Id * powf(max(0.0f, dot(n, h)), p);

    // Final color
    Vector3 color = ambient + diffuse + specular;

    // Gamma correction (¥ã = 2.2)
    color.x = powf(color.x, 1.0f / 2.2f);
    color.y = powf(color.y, 1.0f / 2.2f);
    color.z = powf(color.z, 1.0f / 2.2f);

    // Clamp to [0, 1]
    color.x = std::min(1.0f, std::max(0.0f, color.x));
    color.y = std::min(1.0f, std::max(0.0f, color.y));
    color.z = std::min(1.0f, std::max(0.0f, color.z));

    return color;
}

std::vector<Vector3> vertexNormals;

void calculate_vertex_normals() {
    vertexNormals.resize(gNumVertices, { 0.0f, 0.0f, 0.0f });

    for (int i = 0; i < gNumTriangles; ++i) {
        int idx0 = gIndexBuffer[3 * i + 0];
        int idx1 = gIndexBuffer[3 * i + 1];
        int idx2 = gIndexBuffer[3 * i + 2];

        Vertex v0 = gVertices[idx0];
        Vertex v1 = gVertices[idx1];
        Vertex v2 = gVertices[idx2];

        Vector3 p0 = { v0.x, v0.y, v0.z };
        Vector3 p1 = { v1.x, v1.y, v1.z };
        Vector3 p2 = { v2.x, v2.y, v2.z };

        Vector3 normal = normalize(cross(p1 - p0, p2 - p0));

        vertexNormals[idx0] = vertexNormals[idx0] + normal;
        vertexNormals[idx1] = vertexNormals[idx1] + normal;
        vertexNormals[idx2] = vertexNormals[idx2] + normal;
    }

    for (int i = 0; i < gNumVertices; ++i) {
        vertexNormals[i] = normalize(vertexNormals[i]);
    }
}

void rasterize_triangles() {
    std::fill(&depthBuffer[0][0], &depthBuffer[0][0] + WIDTH * HEIGHT, std::numeric_limits<float>::infinity());
    OutputImage.resize(WIDTH * HEIGHT * 3, 0.0f);

    Vector3 lightPos = { -4.0f, 4.0f, -3.0f };
    Vector3 viewPos = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < gNumTriangles; ++i) {
        int idx0 = gIndexBuffer[3 * i + 0];
        int idx1 = gIndexBuffer[3 * i + 1];
        int idx2 = gIndexBuffer[3 * i + 2];

        Vertex v0 = transformedVertices[idx0];
        Vertex v1 = transformedVertices[idx1];
        Vertex v2 = transformedVertices[idx2];

        Vector3 p0 = { v0.x, v0.y, v0.z };
        Vector3 p1 = { v1.x, v1.y, v1.z };
        Vector3 p2 = { v2.x, v2.y, v2.z };

        Vector3 n0 = vertexNormals[idx0];
        Vector3 n1 = vertexNormals[idx1];
        Vector3 n2 = vertexNormals[idx2];

        Vector3 w0_pos = posWorlds[idx0];
        Vector3 w1_pos = posWorlds[idx1];
        Vector3 w2_pos = posWorlds[idx2];

        int minX = (int)floorf(fmin(fmin(v0.x, v1.x), v2.x));
        int maxX = (int)ceilf(fmax(fmax(v0.x, v1.x), v2.x));
        int minY = (int)floorf(fmin(fmin(v0.y, v1.y), v2.y));
        int maxY = (int)ceilf(fmax(fmax(v0.y, v1.y), v2.y));

        minX = std::max(minX, 0);
        maxX = std::min(maxX, WIDTH - 1);
        minY = std::max(minY, 0);
        maxY = std::min(maxY, HEIGHT - 1);

        float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
        if (denom == 0) continue;

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;

                float w0 = ((v1.y - v2.y) * (px - v2.x) + (v2.x - v1.x) * (py - v2.y)) / denom;
                float w1 = ((v2.y - v0.y) * (px - v2.x) + (v0.x - v2.x) * (py - v2.y)) / denom;
                float w2 = 1.0f - w0 - w1;

                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    float z = w0 * v0.z + w1 * v1.z + w2 * v2.z;

                    if (z < depthBuffer[x][y]) {
                        depthBuffer[x][y] = z;

                        Vector3 interpolatedNormal = normalize(n0 * w0 + n1 * w1 + n2 * w2);
                        Vector3 interpolatedPos = w0_pos * w0 + w1_pos * w1 + w2_pos * w2;
                        Vector3 finalColor = shading(interpolatedNormal, lightPos, viewPos, interpolatedPos);

                        int index = (y * WIDTH + x) * 3;
                        OutputImage[index + 0] = finalColor.x; // R
                        OutputImage[index + 1] = finalColor.y; // G
                        OutputImage[index + 2] = finalColor.z; // B
                    }
                }
            }
        }
    }
}

void create_scene()
{
    int width = 32;
    int height = 16;

    float theta, phi;
    int t;

    gNumVertices = (height - 2) * width + 2;
    gNumTriangles = (height - 2) * (width - 1) * 2;

    // TODO: Allocate an array for gNumVertices vertices.
    gVertices.resize(gNumVertices);

    gIndexBuffer = new int[3 * gNumTriangles];

    t = 0;
    for (int j = 1; j < height - 1; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            theta = (float)j / (height - 1) * M_PI;
            phi = (float)i / (width - 1) * M_PI * 2;

            float   x = sinf(theta) * cosf(phi);
            float   y = cosf(theta);
            float   z = -sinf(theta) * sinf(phi);

            // TODO: Set vertex t in the vertex array to {x, y, z}.
            gVertices[t] = { x, y, z };

            t++;
        }
    }

    // TODO: Set vertex t in the vertex array to {0, 1, 0}.
    gVertices[t] = { 0, 1, 0 };

    t++;

    // TODO: Set vertex t in the vertex array to {0, -1, 0}.
    gVertices[t] = { 0, -1, 0 };

    t++;

    t = 0;
    for (int j = 0; j < height - 3; ++j)
    {
        for (int i = 0; i < width - 1; ++i)
        {
            gIndexBuffer[t++] = j * width + i;
            gIndexBuffer[t++] = (j + 1) * width + (i + 1);
            gIndexBuffer[t++] = j * width + (i + 1);
            gIndexBuffer[t++] = j * width + i;
            gIndexBuffer[t++] = (j + 1) * width + i;
            gIndexBuffer[t++] = (j + 1) * width + (i + 1);
        }
    }
    for (int i = 0; i < width - 1; ++i)
    {
        gIndexBuffer[t++] = (height - 2) * width;
        gIndexBuffer[t++] = i;
        gIndexBuffer[t++] = i + 1;
        gIndexBuffer[t++] = (height - 2) * width + 1;
        gIndexBuffer[t++] = (height - 3) * width + (i + 1);
        gIndexBuffer[t++] = (height - 3) * width + i;
    }

    // The index buffer has now been generated. Here's how to use to determine
    // the vertices of a triangle. Suppose you want to determine the vertices
    // of triangle i, with 0 <= i < gNumTriangles. Define:
    //
    // k0 = gIndexBuffer[3*i + 0]
    // k1 = gIndexBuffer[3*i + 1]
    // k2 = gIndexBuffer[3*i + 2]
    //
    // Now, the vertices of triangle i are at positions k0, k1, and k2 (in that
    // order) in the vertex array (which you should allocate yourself at line
    // 27).
    //
    // Note that this assumes 0-based indexing of arrays (as used in C/C++,
    // Java, etc.) If your language uses 1-based indexing, you will have to
    // add 1 to k0, k1, and k2.
}



//
// main.cpp
// Output Image
//
//

#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>

#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <vector>

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

void render()
{
    //Create our image. We don't want to do this in 
    //the main loop since this may be too slow and we 
    //want a responsive display of our beautiful image.
    //Instead we draw to another buffer and copy this to the 
    //framebuffer using glDrawPixels(...) every refresh
    create_scene();
    calculate_vertex_normals();
    transform_vertices();
    rasterize_triangles();
}

void resize_callback(GLFWwindow*, int nw, int nh)
{
    //This is called in response to the window resizing.
    //The new width and height are passed in so we make 
    //any necessary changes:
    //WIDTH = nw;
    //HEIGHT = nh;
    //Tell the viewport to use all of our screen estate
    glViewport(0, 0, WIDTH, HEIGHT);

    //This is not necessary, we're just working in 2d so
    //why not let our spaces reflect it?
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0.0, static_cast<double>(WIDTH)
        , 0.0, static_cast<double>(HEIGHT)
        , 1.0, -1.0);

    //Reserve memory for our render so that we don't do 
    //excessive allocations and render the image
    OutputImage.reserve(WIDTH * HEIGHT * 3);
    render();
}


int main(int argc, char* argv[])
{
    // -------------------------------------------------
    // Initialize Window
    // -------------------------------------------------

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Viewer", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    //We have an opengl context now. Everything from here on out 
    //is just managing our window or opengl directly.

    //Tell the opengl state machine we don't want it to make 
    //any assumptions about how pixels are aligned in memory 
    //during transfers between host and device (like glDrawPixels(...) )
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    //We call our resize function once to set everything up initially
    //after registering it as a callback with glfw
    glfwSetFramebufferSizeCallback(window, resize_callback);
    resize_callback(NULL, WIDTH, HEIGHT);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        //Clear the screen
        glClear(GL_COLOR_BUFFER_BIT);

        // -------------------------------------------------------------
        //Rendering begins!
        glDrawPixels(WIDTH, HEIGHT, GL_RGB, GL_FLOAT, &OutputImage[0]);
        //and ends.
        // -------------------------------------------------------------

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        //Close when the user hits 'q' or escape
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
