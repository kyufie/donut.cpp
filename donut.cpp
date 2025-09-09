#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>
#include <unistd.h>

class Fb {
public:
  unsigned int width;
  unsigned int height;

  Fb(unsigned int width, unsigned int height) : width(width), height(height) {
    buf = (char *)malloc(width * height);
    if (buf == nullptr) {
      throw std::bad_alloc();
    }
    clear();
  }

  ~Fb() { free(buf); }

  inline void clear() { memset(buf, ' ', width * height); }

  inline char get(unsigned int x, unsigned int y) {
    assert(x < width && y < height);
    return buf[y * width + x];
  }

  inline void set(char c, unsigned int x, unsigned int y) {
    assert(x < width && y < height);
    buf[y * width + x] = c;
  }

private:
  char *buf;
};

class ZBuf {
public:
  unsigned int width;
  unsigned int height;

  ZBuf(unsigned int width, unsigned int height) : width(width), height(height) {
    buf = (float *)malloc(width * height * sizeof(float));
    if (buf == nullptr) {
      throw std::bad_alloc();
    }
    clear();
  }

  inline void clear() { memset(buf, 0, width * height * sizeof(float)); }

  inline float get(unsigned int x, unsigned int y) {
    assert(x < width && y < height);
    return buf[y * width + x];
  }

  inline bool set(float v, unsigned int x, unsigned int y) {
    assert(x < width && y < height);
    if (v > get(x, y)) {
      buf[y * width + x] = v;
      return true;
    }
    return false;
  }

  ~ZBuf() { free(buf); }

private:
  float *buf;
};

void print_fb(Fb &fb) {
  for (int y = 0; y < fb.height; y++) {
    for (int x = 0; x < fb.width; x++) {
      putchar(fb.get(x, y));
    }
    putchar('\n');
  }
}

inline void clear_terminal() { printf("\x1b[H"); }

// Draw a torus to a framebuffer
void draw_torus(Fb &fb, ZBuf &zbuf, unsigned int r1, unsigned int r2,
                float z_offset, float angle_step_theta, float angle_step_phi,
                float rot_x, float rot_z) {
  const float sin_a = sin(rot_x), cos_a = cos(rot_x);
  const float sin_b = sin(rot_z), cos_b = cos(rot_z);

  // Ensure that the torus is scaled so that the longest part of the torus
  // (calculated at z = 0) fill 6/8-th of the framebuffer.
  // screen_width * 3/8 = mult * (r1 + r2) / (z_offset + z)
  // screen_width * 3/8 = mult * (r1 + r2) / (z_offset + 0)
  const float mult_x = fb.width * z_offset * 3 / (8 * (r1 + r2));
  const float mult_y = fb.height * z_offset * 3 / (8 * (r1 + r2));

  fb.clear();
  zbuf.clear();

  for (float theta = 0; theta < 2 * M_PI; theta += angle_step_theta) {
    const float cos_theta = cos(theta), sin_theta = sin(theta);

    for (float phi = 0; phi < 2 * M_PI; phi += angle_step_phi) {
      float sin_phi = sin(phi), cos_phi = cos(phi);

      // Draw the torus' cross-section circle.
      float circle_x = r1 + r2 * cos_phi;
      float circle_y = r2 * sin_phi;

      // Revolve the cross-section point around the torus' central axis in three
      // dimensions.
      float torus_x = circle_x * cos_theta;
      float torus_y = circle_y;
      float torus_z = circle_x * -sin_theta;

      // For the animation, do two more rotations around the x and z axes.
      float torus_x2 = torus_x;
      float torus_y2 = torus_y * cos_a + torus_z * sin_a;
      float torus_z2 = torus_z * cos_a - torus_y * sin_a;

      float torus_x3 = torus_x2 * cos_b + torus_y2 * sin_b;
      float torus_y3 = torus_x2 * -sin_b + torus_y2 * cos_b;
      float torus_z3 = z_offset + torus_z2;

      // Precompute part of the scale value.
      // Division can be slow, to minimize its effect, we do one division
      // here and use the resulting value to do two divisions using
      // multiplication, which is said to be much faster compared to two
      // divisions. However, the difference here may not be as meaningful, I'm
      // not sure.
      float ooz = 1 / torus_z3;

      // Move the coordinate origin to the center of the screen and scale all
      // points. The y axis is flipped because y goes up in 3D space but down in
      // 2D displays.
      int screen_x = (float)fb.width / 2 + mult_x * ooz * torus_x3;
      int screen_y = (float)fb.height / 2 - mult_y * ooz * torus_y3;

      // Calculate the surface normal for luminance. This is the direction at
      // which a point is facing.
      float n_x1 = cos_phi * cos_theta;
      float n_y1 = sin_phi;
      float n_z1 = -sin_theta * cos_phi;

      // Rotate the surface normal the same way points are rotated.
      float n_x2 = n_x1;
      float n_y2 = n_y1 * cos_a + n_z1 * sin_a;
      float n_z2 = -sin_a * n_y1 + n_z1 * cos_a;

      float n_y3 = n_x2 * -sin_b + n_y2 * cos_b;
      float n_z3 = n_z2;

      // Calculate luminance using the dot product between the surface
      // normal and a fixed vector [0, 1, -1]. In other words, illuminate the
      // surface that points to the direction above and behind the viewer, the
      // closer the surface direction matches the given direction, the brighter
      // it gets.
      // luminance = x(0) + y(1) + z(-1)
      float luminance = n_y3 - n_z3;

      // Check if the current point is eligible for plotting. If the point is
      // located behind what's already plotted, then ignore it.
      if (zbuf.set(ooz, screen_x, screen_y)) {
        // Make sure that every plotted pixels can always be seen, no matter how
        // dim they are, so we can always see the complete shape of the torus.
        int luminance_index = luminance < 0 ? 0 : luminance * 8;
        fb.set(".,-~:;=!*#$@"[luminance_index], screen_x, screen_y);
      }
    }
  }
}

int main() {
  // 2:1 aspect ratio is chosen due to the size of the characters in terminal
  // not being a perfect square.
  const unsigned int screen_width = 50, screen_height = 25;
  const int r1 = 2, r2 = 1;
  const float z_offset = 5;

  Fb fb(screen_width, screen_height);
  ZBuf zbuf(screen_width, screen_height);

  float rot = 0;
  while (true) {
    clear_terminal();
    draw_torus(fb, zbuf, r1, r2, z_offset, 0.02, 0.02, rot * 2, rot);
    print_fb(fb);
    usleep(30000);
    rot += 0.02;
  }
}
