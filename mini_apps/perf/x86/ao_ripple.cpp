//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple.h>
#include <ripple_math.h>
#include <stdlib.h>
#include <string.h>

#include <ripple-test-suite/ripple-test-suite.h>

#define NAO_SAMPLES 8
#define M_PI 3.141592653f

static const uint64_t RAND48_MULT = 0x5DEECE66DULL;
static const uint64_t RAND48_ADD = 0xBULL;
static const uint64_t RAND48_MASK = ((1ULL << 48) - 1);

inline float __attribute__((always_inline))
drand48_ripple(uint64_t *rand_state) {
  *rand_state = (RAND48_MULT * (*rand_state) + RAND48_ADD) & RAND48_MASK;
  return (float)(*rand_state) / (float)(1ULL << 48);
}

typedef struct _vec {
  float x;
  float _pad1;
  float y;
  float _pad2;
  float z;
} vec;

typedef struct _Isect {
  float t;
  vec p;
  vec n;
  int hit;
} Isect;

typedef struct _Sphere {
  vec center;
  float radius;

} Sphere;

typedef struct _Plane {
  vec p;
  vec n;

} Plane;

typedef struct _Ray {
  vec org;
  vec dir;
} Ray;

inline float __attribute__((always_inline)) vdot(vec v0, vec v1) {
  return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
}

inline void __attribute__((always_inline)) vcross(vec *c, vec v0, vec v1) {

  c->x = v0.y * v1.z - v0.z * v1.y;
  c->y = v0.z * v1.x - v0.x * v1.z;
  c->z = v0.x * v1.y - v0.y * v1.x;
}

inline void __attribute__((always_inline)) vnormalize(vec *c) {
  float length = sqrtf(vdot((*c), (*c)));

  float abs_length = length > 0 ? length : -length;

  if (abs_length > 1.0e-17f) {
    c->x /= length;
    c->y /= length;
    c->z /= length;
  }
}

inline void __attribute__((always_inline))
ray_sphere_intersect(Isect *isect, const Ray *ray, const Sphere *sphere) {
  vec rs;

  rs.x = ray->org.x - sphere->center.x;
  rs.y = ray->org.y - sphere->center.y;
  rs.z = ray->org.z - sphere->center.z;

  float B = vdot(rs, ray->dir);
  float C = vdot(rs, rs) - sphere->radius * sphere->radius;
  float D = B * B - C;

  if (D > 0.0) {
    float t = -B - sqrtf(D);

    if ((t > 0.0) && (t < isect->t)) {
      isect->t = t;
      isect->hit = 1;

      isect->p.x = ray->org.x + ray->dir.x * t;
      isect->p.y = ray->org.y + ray->dir.y * t;
      isect->p.z = ray->org.z + ray->dir.z * t;

      isect->n.x = isect->p.x - sphere->center.x;
      isect->n.y = isect->p.y - sphere->center.y;
      isect->n.z = isect->p.z - sphere->center.z;

      vnormalize(&(isect->n));
    }
  }
}

inline unsigned char __attribute__((always_inline)) clamp(float f) {
  int i = (int)(f * 255.5f);

  if (i < 0)
    i = 0;
  if (i > 255)
    i = 255;

  return (unsigned char)i;
}

inline void __attribute__((always_inline))
ray_plane_intersect(ripple_block_t BS, Isect *isect, const Ray *ray,
                    const Plane *plane) {
  float d = -vdot(plane->p, plane->n);
  float v = vdot(ray->dir, plane->n);

  if (fabsf(v) < 1.0e-17f)
    return;

  float t = -(vdot(ray->org, plane->n) + d) / v;

  if ((t > 0.0) && (t < isect->t)) {
    isect->t = t;
    isect->hit = ripple_broadcast(BS, 0b1, 1);

    isect->p.x = ray->org.x + ray->dir.x * t;
    isect->p.y = ray->org.y + ray->dir.y * t;
    isect->p.z = ray->org.z + ray->dir.z * t;

    isect->n.x = plane->n.x;
    isect->n.y = plane->n.y;
    isect->n.z = plane->n.z;
  }
}

inline void __attribute__((always_inline))
orthoBasis_ripple(ripple_block_t BS, vec *basis, vec n) {
  basis[2] = n;
  float zero_v = ripple_broadcast(BS, 0b1, 0.0f);
  float one_v = ripple_broadcast(BS, 0b1, 1.0f);
  basis[1].x = zero_v;
  basis[1].y = zero_v;
  basis[1].z = zero_v;

  if ((n.x < 0.6f) && (n.x > -0.6f)) {
    basis[1].x = one_v;
  } else if ((n.y < 0.6f) && (n.y > -0.6f)) {
    basis[1].y = one_v;
  } else if ((n.z < 0.6f) && (n.z > -0.6f)) {
    basis[1].z = one_v;
  } else {
    basis[1].x = one_v;
  }

  vcross(&basis[0], basis[1], basis[2]);
  vnormalize(&basis[0]);

  vcross(&basis[1], basis[2], basis[0]);
  vnormalize(&basis[1]);
}

inline void __attribute__((always_inline))
ambient_occlusion(ripple_block_t BS, vec *col, uint64_t *rand48_state,
                  const Isect *isect, const Sphere *spheres,
                  const Plane *plane) {
  int i, j;
  int ntheta = NAO_SAMPLES;
  int nphi = NAO_SAMPLES;
  float eps = 0.0001f;
  size_t v0 = ripple_id(BS, 0);

  vec p;

  p.x = isect->p.x + eps * isect->n.x;
  p.y = isect->p.y + eps * isect->n.y;
  p.z = isect->p.z + eps * isect->n.z;
  float zero_v = ripple_broadcast(BS, 0b1, 0.0f);

  vec basis[3];
  orthoBasis_ripple(BS, basis, isect->n);

  float occlusion = zero_v;

  for (j = 0; j < ntheta; j++) {
    for (i = 0; i < nphi; i++) {
      float theta = sqrtf(drand48_ripple(&rand48_state[v0]));
      float phi = 2.0f * M_PI * drand48_ripple(&rand48_state[v0]);

      float x = cosf(phi) * theta;
      float y = sinf(phi) * theta;
      float z = sqrtf(1.0f - theta * theta);

      // local -> global
      float rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
      float ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
      float rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

      Ray ray;

      ray.org = p;
      ray.dir.x = rx;
      ray.dir.y = ry;
      ray.dir.z = rz;

      Isect occIsect;
      occIsect.t = 1.0e+17f;
      occIsect.hit = zero_v;

      ray_sphere_intersect(&occIsect, &ray, &spheres[0]);
      ray_sphere_intersect(&occIsect, &ray, &spheres[1]);
      ray_sphere_intersect(&occIsect, &ray, &spheres[2]);
      ray_plane_intersect(BS, &occIsect, &ray, plane);

      if (occIsect.hit)
        occlusion += 1;
    }
  }

  occlusion = (ntheta * nphi - occlusion) / (float)(ntheta * nphi);

  col->x = occlusion;
  col->y = occlusion;
  col->z = occlusion;
}

// Only valid for block size 8.
size_t src_fimg_r_fn(size_t i, size_t n) { return (3 * i) % 8; }

// Only valid for block size 8.
size_t src_fimg_g_fn(size_t i, size_t n) { return (3 * i + 5) % 8; }

// Only valid for block size 8.
size_t src_fimg_b_fn(size_t i, size_t n) { return (3 * i + 2) % 8; }

static void __attribute__((noinline))
render(unsigned char *img, const Sphere *spheres, const Plane *plane, int w,
       int h, int nsubsamples) {
  constexpr size_t nv0 = 8;
  ripple_block_t BS = ripple_set_block_shape(0, 8);
  size_t v0 = ripple_id(BS, 0);

  uint64_t rand_state[8] = {
      (0x1234ULL << 32) | 0x330EULL, // Seed 1
      (0x2345ULL << 32) | 0x440FULL, // Seed 2
      (0x3456ULL << 32) | 0x5510ULL, // Seed 3
      (0x4567ULL << 32) | 0x6611ULL, // Seed 4
      (0x5678ULL << 32) | 0x7722ULL, // Seed 5
      (0x6789ULL << 32) | 0x8833ULL, // Seed 6
      (0x789AULL << 32) | 0x9944ULL, // Seed 7
      (0x89ABULL << 32) | 0xAA55ULL  // Seed 8
  };

  for (int y = 0; y < h; y++) {
    for (int x = 0; x + nv0 <= w; x += nv0) {
      float fimg_r = ripple_broadcast(BS, 0b1, 0.f);
      float fimg_g = ripple_broadcast(BS, 0b1, 0.f);
      float fimg_b = ripple_broadcast(BS, 0b1, 0.f);

      for (int v = 0; v < nsubsamples; v++) {
        for (int u = 0; u < nsubsamples; u++) {
          float px =
              (x + v0 + (u / (float)nsubsamples) - (w / 2.0f)) / (w / 2.0f);
          float py = -(y + (v / (float)nsubsamples) - (h / 2.0f)) / (h / 2.0f);

          Ray ray;

          ray.org.x = 0.0f;
          ray.org.y = 0.0f;
          ray.org.z = 0.0f;

          ray.dir.x = px;
          ray.dir.y = py;
          ray.dir.z = -1.0f;
          vnormalize(&(ray.dir));

          Isect isect;

          // Note that it is necessary to broadcast as inside
          // `ray_sphere_interesct`, it is updated with a scalar value which is
          // predicated with a vector-conditional. We have to tell Ripple
          // explicitly that this is vector value, as Ripple's specfication
          // expands to tensors based solely on the value (not control flow.)
          isect.t = ripple_broadcast(BS, 0b1, 1.0e+17f);
          isect.hit = ripple_broadcast(BS, 0b1, 0);
          isect.p.x = ripple_broadcast(BS, 0b1, 0.f);
          isect.p.y = ripple_broadcast(BS, 0b1, 0.f);
          isect.p.z = ripple_broadcast(BS, 0b1, 0.f);
          isect.n.x = ripple_broadcast(BS, 0b1, 0.f);
          isect.n.y = ripple_broadcast(BS, 0b1, 0.f);
          isect.n.z = ripple_broadcast(BS, 0b1, 0.f);

          ray_sphere_intersect(&isect, &ray, &spheres[0]);
          ray_sphere_intersect(&isect, &ray, &spheres[1]);
          ray_sphere_intersect(&isect, &ray, &spheres[2]);
          ray_plane_intersect(BS, &isect, &ray, plane);

          if (isect.hit) {
            vec col = {ripple_broadcast(BS, 0b1, 0.f),
                       ripple_broadcast(BS, 0b1, 0.f),
                       ripple_broadcast(BS, 0b1, 0.f)};
            ambient_occlusion(BS, &col, rand_state, &isect, spheres, plane);

            fimg_r += col.x;
            fimg_g += col.y;
            fimg_b += col.z;
          }
        }
      }

      fimg_r /= (float)(nsubsamples * nsubsamples);
      fimg_g /= (float)(nsubsamples * nsubsamples);
      fimg_b /= (float)(nsubsamples * nsubsamples);

      unsigned char fimg_r_c = clamp(fimg_r);
      unsigned char fimg_g_c = clamp(fimg_g);
      unsigned char fimg_b_c = clamp(fimg_b);

      // img[3 * (y * w + x + v0) + 0] = fimg_r_c;
      // img[3 * (y * w + x + v0) + 1] = fimg_g_c;
      // img[3 * (y * w + x + v0) + 2] = fimg_b_c;

      fimg_r_c = ripple_shuffle(fimg_r_c, src_fimg_r_fn);
      fimg_g_c = ripple_shuffle(fimg_g_c, src_fimg_g_fn);
      fimg_b_c = ripple_shuffle(fimg_b_c, src_fimg_b_fn);

      uint8_t mask_0 = v0 % 3;
      uint8_t mask_1 = (v0 + 8) % 3;
      uint8_t mask_2 = (v0 + 16) % 3;
      unsigned char val_to_store_0 =
          (mask_0 == 0) ? fimg_r_c : ((mask_0 == 1) ? fimg_g_c : fimg_b_c);
      unsigned char val_to_store_1 =
          (mask_1 == 0) ? fimg_r_c : ((mask_1 == 1) ? fimg_g_c : fimg_b_c);
      unsigned char val_to_store_2 =
          (mask_2 == 0) ? fimg_r_c : ((mask_2 == 1) ? fimg_g_c : fimg_b_c);

      img[3 * (y * w + x) + v0] = val_to_store_0;
      img[3 * (y * w + x) + v0 + 8] = val_to_store_1;
      img[3 * (y * w + x) + v0 + 16] = val_to_store_2;
    }
  }
}

namespace ripple_test_suite {

class AmbientOcclussion : public Test {
  static constexpr int height = 256;
  static constexpr int width = 256;
  static constexpr int nsubsamples = 2;
  Sphere spheres[3];
  Plane plane;
  unsigned char img[height * width * 3];

public:
  AmbientOcclussion(TestFramework &TestFramework) : Test(TestFramework) {
    spheres[0].center.x = -2.0f;
    spheres[0].center.y = 0.0f;
    spheres[0].center.z = -3.5f;
    spheres[0].radius = 0.5f;

    spheres[1].center.x = -0.5f;
    spheres[1].center.y = 0.0f;
    spheres[1].center.z = -3.0f;
    spheres[1].radius = 0.5f;

    spheres[2].center.x = 1.0f;
    spheres[2].center.y = 0.0f;
    spheres[2].center.z = -2.2f;
    spheres[2].radius = 0.5f;

    plane.p.x = 0.0f;
    plane.p.y = -0.5f;
    plane.p.z = 0.0f;

    plane.n.x = 0.0f;
    plane.n.y = 1.0f;
    plane.n.z = 0.0f;

    memset(img, 0, width * height * 3);
  }

  void run(unsigned) override {
    render(img, spheres, &plane, width, height, nsubsamples);
  }

  bool verify() const override {
    // TODO: Because of the use of a PRNG within the inner loop,
    // comparing with reference is not quite possible.
    // One idea is to pre-generate random numbers into a buffer
    // and accept it as an input.
    return true;
  }
};

extern "C" void ao_ispc(int w, int h, int nsubsamples, uint8_t image[]);

class AmbientOcclussionISPC : public Test {
  static constexpr int height = 256;
  static constexpr int width = 256;
  static constexpr int nsubsamples = 2;
  uint8_t img[height * width * 3];

public:
  AmbientOcclussionISPC(TestFramework &TestFramework) : Test(TestFramework) {
    memset(img, 0, width * height * 3);
  }

  void run(unsigned) override { ao_ispc(width, height, nsubsamples, img); }

  bool verify() const override {
    // TODO: Because of the use of a PRNG within the inner loop,
    // comparing with reference is not quite possible.
    // One idea is to pre-generate random numbers into a buffer
    // and accept it as an input.
    return true;
  }
};

DefineTest<AmbientOcclussionISPC>
    AmbientOcclusion_Ripple_0("ambient_occlusion.ispc");
DefineTest<AmbientOcclussion>
    AmbientOcclusion_Ripple_1("ambient_occlusion.ripple");
} // namespace ripple_test_suite
