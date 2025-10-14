#include "../src/sunburst.h"
#include <stdio.h>

static const char *vs_410 =
"#version 410 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main(){ gl_Position = vec4(aPos,1.0); }\n";

static const char *fs_410 =
"#version 410 core\n"
"out vec4 FragColor;\n"
"void main(){ FragColor = vec4(1.0,0.5,0.2,1.0); }\n";

int main(void) {
    // 1) Create window + GL context first
    if (!InitWindow(1600, 800, "Triangle")) return 1;
    if (!CreateGLContext()) return 2;
    GL_SetSwapInterval(0); // vsync

    const char *vsrc = vs_410, *fsrc = fs_410; 

    // 3) Build program
    GLuint vs = compile(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked=0; glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if(!linked){
        char log[1024]; GLsizei n=0;
        glGetProgramInfoLog(prog, sizeof log, &n, log);
        fprintf(stderr, "link error: %.*s\n", (int)n, log);
    }
    glDeleteShader(vs); glDeleteShader(fs);

    // 4) Geometry
    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
         0.5f,  0.5f, 0.0f,  // top right
         0.5f, -0.5f, 0.0f,  // bottom right
        -0.5f, -0.5f, 0.0f,  // bottom left
        -0.5f,  0.5f, 0.0f,   // top left 
         0.8f, -0.3f, 0.0f,
    };
    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 3,  // first Triangle
        1, 2, 3,   // second Triangle
        1, 2, 4
    };
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    #if defined(__APPLE__) || defined(_WIN32)
        // core profile requires a VAO
        glGenVertexArrays(1,&VAO);
        glBindVertexArray(VAO);
    #endif

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0); 





    // 5) Loop
    while (!WindowShouldClose()) {
        PollEvents();

        int fbw=0, fbh=0;
        GetFramebufferSize(&fbw,&fbh);
        glViewport(0,0,fbw,fbh);
        glClearColor(0.08f,0.09f,0.11f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
#if defined(__APPLE__) || defined(_WIN32)
        glBindVertexArray(VAO);
#endif
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDrawElements(GL_TRIANGLES, 9, GL_UNSIGNED_INT, 0);
        GL_SwapBuffers();
        PrintFrameRate();
    }

    CloseWindowSB();
    return 0;
}
