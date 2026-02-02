//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <math.h>
#include <ripple-test-suite/ripple-test-suite.h>
#include <stdlib.h>
#include <string.h>

#define NAO_SAMPLES 8

typedef struct _vec {
  float x;
  float y;
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

  if (fabsf(length) > 1.0e-17f) {
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

  if (D > 0.0f) {
    float t = -B - sqrtf(D);

    if ((t > 0.0f) && (t < isect->t)) {
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

inline void __attribute__((always_inline))
ray_plane_intersect(Isect *isect, const Ray *ray, const Plane *plane) {
  float d = -vdot(plane->p, plane->n);
  float v = vdot(ray->dir, plane->n);

  if (fabsf(v) < 1.0e-17f)
    return;

  float t = -(vdot(ray->org, plane->n) + d) / v;

  if ((t > 0.0f) && (t < isect->t)) {
    isect->t = t;
    isect->hit = 1;

    isect->p.x = ray->org.x + ray->dir.x * t;
    isect->p.y = ray->org.y + ray->dir.y * t;
    isect->p.z = ray->org.z + ray->dir.z * t;

    isect->n = plane->n;
  }
}

inline void __attribute__((always_inline)) orthoBasis(vec *basis, vec n) {
  basis[2] = n;
  basis[1].x = 0.0f;
  basis[1].y = 0.0f;
  basis[1].z = 0.0f;

  if ((n.x < 0.6f) && (n.x > -0.6f)) {
    basis[1].x = 1.0f;
  } else if ((n.y < 0.6f) && (n.y > -0.6f)) {
    basis[1].y = 1.0f;
  } else if ((n.z < 0.6f) && (n.z > -0.6f)) {
    basis[1].z = 1.0f;
  } else {
    basis[1].x = 1.0f;
  }

  vcross(&basis[0], basis[1], basis[2]);
  vnormalize(&basis[0]);

  vcross(&basis[1], basis[2], basis[0]);
  vnormalize(&basis[1]);
}

inline void __attribute__((always_inline))
ambient_occlusion(vec *col, const Isect *isect, const Sphere *spheres,
                  const Plane *plane) {
  int i, j;
  int ntheta = NAO_SAMPLES;
  int nphi = NAO_SAMPLES;
  float eps = 0.0001f;

  vec p;

  p.x = isect->p.x + eps * isect->n.x;
  p.y = isect->p.y + eps * isect->n.y;
  p.z = isect->p.z + eps * isect->n.z;

  vec basis[3];
  orthoBasis(basis, isect->n);

  float occlusion = 0.0f;

  for (j = 0; j < ntheta; j++) {
    for (i = 0; i < nphi; i++) {
      float theta = sqrtf(drand48());
      float phi = 2.0f * M_PI * drand48();

      float x = cosf(phi) * theta;
      float y = sinf(phi) * theta;
      float z = sqrtf(1.0 - theta * theta);

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
      occIsect.hit = 0;

      ray_sphere_intersect(&occIsect, &ray, &spheres[0]);
      ray_sphere_intersect(&occIsect, &ray, &spheres[1]);
      ray_sphere_intersect(&occIsect, &ray, &spheres[2]);
      ray_plane_intersect(&occIsect, &ray, plane);

      if (occIsect.hit)
        occlusion += 1.0;
    }
  }

  occlusion = (ntheta * nphi - occlusion) / (float)(ntheta * nphi);

  col->x = occlusion;
  col->y = occlusion;
  col->z = occlusion;
}

inline unsigned char __attribute__((always_inline)) clamp(float f) {
  int i = (int)(f * 255.5f);

  if (i < 0)
    i = 0;
  if (i > 255)
    i = 255;

  return (unsigned char)i;
}

static void render(unsigned char *img, const Sphere *spheres,
                   const Plane *plane, int w, int h, int nsubsamples) {
  int x, y;
  int u, v;

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      float fimg_r = 0;
      float fimg_g = 0;
      float fimg_b = 0;

      for (v = 0; v < nsubsamples; v++) {
        for (u = 0; u < nsubsamples; u++) {
          float px = (x + (u / (float)nsubsamples) - (w / 2.0)) / (w / 2.0);
          float py = -(y + (v / (float)nsubsamples) - (h / 2.0)) / (h / 2.0);

          Ray ray;

          ray.org.x = 0.0f;
          ray.org.y = 0.0f;
          ray.org.z = 0.0f;

          ray.dir.x = px;
          ray.dir.y = py;
          ray.dir.z = -1.0f;
          vnormalize(&(ray.dir));

          Isect isect;
          isect.t = 1.0e+17f;
          isect.hit = 0;

          ray_sphere_intersect(&isect, &ray, &spheres[0]);
          ray_sphere_intersect(&isect, &ray, &spheres[1]);
          ray_sphere_intersect(&isect, &ray, &spheres[2]);
          ray_plane_intersect(&isect, &ray, plane);

          if (isect.hit) {
            vec col;
            ambient_occlusion(&col, &isect, spheres, plane);

            fimg_r += col.x;
            fimg_g += col.y;
            fimg_b += col.z;
          }
        }
      }

      fimg_r /= (float)(nsubsamples * nsubsamples);
      fimg_g /= (float)(nsubsamples * nsubsamples);
      fimg_b /= (float)(nsubsamples * nsubsamples);

      img[3 * (y * w + x) + 0] = clamp(fimg_r);
      img[3 * (y * w + x) + 1] = clamp(fimg_g);
      img[3 * (y * w + x) + 2] = clamp(fimg_b);
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
  uint8_t img[height * width * 3];

public:
  AmbientOcclussion(TestFramework &TestFramework) : Test(TestFramework) {
    spheres[0].center.x = -2.0;
    spheres[0].center.y = 0.0;
    spheres[0].center.z = -3.5;
    spheres[0].radius = 0.5;

    spheres[1].center.x = -0.5;
    spheres[1].center.y = 0.0;
    spheres[1].center.z = -3.0;
    spheres[1].radius = 0.5;

    spheres[2].center.x = 1.0;
    spheres[2].center.y = 0.0;
    spheres[2].center.z = -2.2;
    spheres[2].radius = 0.5;

    plane.p.x = 0.0;
    plane.p.y = -0.5;
    plane.p.z = 0.0;

    plane.n.x = 0.0;
    plane.n.y = 1.0;
    plane.n.z = 0.0;

    memset(img, 0, width * height * 3);
  }

  void run(unsigned) override {
    render(img, spheres, &plane, width, height, nsubsamples);
  }

  bool verify() const override {
    // TODO: Because of the use of a PRNG within the inner loop,
    // comparing with reference is not quite possible.
    return true;
  }
};

DefineTest<AmbientOcclussion> AmbientOcclusion_Ref("ambient_occlusion.ref");

} // namespace ripple_test_suite
