#define GL_SILENCE_DEPRECATION
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#endif
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define KB  * (1024)
#define MB  * (1048576)
#define GB  * (u64)(1073741824)

typedef unsigned long u64;

typedef struct String_Slice
{
  char* data;
  int   length;
} String_Slice;

static inline String_Slice  String_Slice_empty(void)
{
  return (String_Slice) {.data = NULL, .length = 0 };
}

static inline int String_Slice_is_valid(String_Slice slice)
{
  return (slice.data != NULL);
}

typedef struct Arena
{
  char* start_ptr;
  char* fill_ptr;
  u64   capacity;
} Arena;

typedef char* Arena_Snapshot;

static void Arena_init(Arena* arena, u64 capacity)
{
  arena->start_ptr = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  arena->fill_ptr = arena->start_ptr;
  arena->capacity = capacity;
}

static void* Arena_alloc(Arena* arena, u64 bytes)
{
  if ((arena->fill_ptr - arena->start_ptr + bytes) > arena->capacity)
  {
    printf("Rejected request to get %ld bytes from arena. %ld bytes left in arena\n", bytes,  arena->capacity - (arena->fill_ptr - arena->start_ptr));
    return NULL;
  }
  void* memory = arena->fill_ptr;
  arena->fill_ptr += bytes;
  return memory;
}

static void Arena_clear(Arena* arena)
{
  arena->fill_ptr = arena->start_ptr;
}

static Arena_Snapshot Arena_snapshot(Arena* arena)
{
  return arena->fill_ptr;
}

static void Arena_rollback(Arena* arena, Arena_Snapshot snapshot)
{
  arena->fill_ptr = snapshot;
}

Arena arena_all;

static String_Slice  load_file(const char* path)
{
  int fd = open(path, O_RDONLY, 0777);
  if (fd == -1)
  {
    perror("openglfun");
    return String_Slice_empty();
  }
  struct stat file_info;

  if (fstat(fd, &file_info) == -1)
  {
    perror("openglfun");
    close(fd);
    return String_Slice_empty();
  }
  char* data = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return (String_Slice) {.data = data, .length = file_info.st_size };
}

static unsigned int compile_shader(unsigned int type, String_Slice source)
{
  unsigned int  id = glCreateShader(type);
  glShaderSource(id, 1, (const char**) &source.data, &source.length);
  glCompileShader(id);

  int result;
  glGetShaderiv(id, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE)
  {
    int length;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    Arena_Snapshot snapshot = Arena_snapshot(&arena_all);
    char* message = Arena_alloc(&arena_all, length * sizeof(char));
    glGetShaderInfoLog(id, length,  &length, message);
    printf("Failed to compile %s shader. Error message:\n%s\n",
        (type == GL_VERTEX_SHADER ? "vertex" : "fragment"),
        message);
    Arena_rollback(&arena_all, snapshot);
    glDeleteShader(id);
    return 0;
  }
  return id;
}

static unsigned int create_shader_program(String_Slice vertex_shader_source, String_Slice fragment_shader_source)
{
  unsigned int program = glCreateProgram();

  unsigned int vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
  unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glValidateProgram(program);

  glDeleteShader(vs);
  glDeleteShader(fs);
  return program;
}

int main(void)
{
    GLFWwindow* window;

    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    printf("Running on %s\n", glGetString(GL_VERSION));
    Arena_init(&arena_all, 1 MB);

    unsigned int program = 0;
    {
      String_Slice  vertex_shader_source = load_file("./shaders/vertex.glsl");
      String_Slice  fragment_shader_source = load_file("./shaders/fragment.glsl");
      if (!String_Slice_is_valid(vertex_shader_source) || !String_Slice_is_valid(fragment_shader_source))
      {
        glfwTerminate();
        return 1;
      }
      program = create_shader_program(vertex_shader_source, fragment_shader_source);
      munmap(vertex_shader_source.data, vertex_shader_source.length);
      munmap(fragment_shader_source.data, fragment_shader_source.length);
    }
    glUseProgram(program);
    Arena_clear(&arena_all);

    unsigned int vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    float positions[8] = {
      -0.5f, -0.5f,
       0.5f, -0.5f,
       0.5f,  0.5f,
      -0.5f,  0.5f
    };

    unsigned int  indices[6] = {
      0, 1, 2,
      2, 3, 0
    };

    unsigned int ibo;
    unsigned int buffer_id;
    glGenBuffers(1, &buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), positions, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned int), indices, GL_STATIC_DRAW);

    glBindVertexArray(vao);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }
    glDeleteProgram(program);
    glfwTerminate();
    return 0;
}

