#pragma once

#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace pdfcube {

struct WiperVertex { float x, y, z, light; };
struct WiperRange { int first, count; };
struct WiperMesh {
  std::array<WiperVertex, 4096> vertices{};
  std::array<WiperRange, 3> materials{}; // Rubber, painted frame, steel.
  int count{0};
};

inline float wiper_flex(double progress) {
  constexpr double pi = 3.14159265358979323846;
  const double p = std::clamp(progress, 0.0, 1.0);
  if (p <= 0 || p >= 1) return 0;
  // A small damped response, evaluated directly rather than integrated.
  return static_cast<float>(.07 * std::sin(5*pi*p) * std::sin(pi*p) * std::exp(-3*p));
}

// Fixed-capacity, closed, lit meshes. All part geometry is regenerated in-place;
// there are no per-frame allocations, random values or accumulated simulation state.
inline void update_wiper_mesh(double progress, float half, int direction, WiperMesh& mesh) {
  constexpr float pi = 3.14159265358979323846F;
  using Point = std::array<float,3>;
  mesh.count = 0;
  const float p = static_cast<float>(std::clamp(progress,0.0,1.0));
  const Matrix flex = translate(-half*.22F,0,.018F) * rotate_y(.48F + wiper_flex(p));
  const float angle = direction * pi / 180;
  const float cosine = std::cos(angle), sine = std::sin(angle);
  const auto height = [&](float y) {
    const float bow = std::max(0.0F,1-y*y/(1.14F*1.14F));
    return .026F + (.055F + .006F*std::sin(pi*p))*bow;
  };
  for (int material=0; material<3; ++material) {
    const int first = mesh.count;
    const auto lighting = [&](const Matrix& model, Point normal) {
      const auto& m = model.value;
      const float nx=m[0]*normal[0]+m[4]*normal[1]+m[8]*normal[2];
      const float ny=m[1]*normal[0]+m[5]*normal[1]+m[9]*normal[2];
      const float nz=m[2]*normal[0]+m[6]*normal[1]+m[10]*normal[2];
      const float world_x=cosine*nx-sine*ny, world_y=sine*nx+cosine*ny;
      const float diffuse=std::max(0.0F,-.42F*world_x+.36F*world_y+.83F*nz);
      const float highlight=.14F*std::pow(std::max(nz,0.0F),16);
      return .30F+.65F*diffuse+highlight;
    };
    const auto position = [&](const Matrix& model, Point point, float light) {
      if (mesh.count >= static_cast<int>(mesh.vertices.size()))
        throw std::length_error("wiper mesh exceeded fixed capacity");
      const auto& m = model.value;
      const float x=m[0]*point[0]+m[4]*point[1]+m[8]*point[2]+m[12];
      const float y=m[1]*point[0]+m[5]*point[1]+m[9]*point[2]+m[13];
      const float z=m[2]*point[0]+m[6]*point[1]+m[10]*point[2]+m[14];
      mesh.vertices[mesh.count++]={x,y,z,light};
    };
    const auto vertex = [&](const Matrix& model, Point point, Point normal) {
      position(model,point,lighting(model,normal));
    };
    const auto quad = [&](const Matrix& model, std::array<Point,4> corners, Point normal) {
      const float light=lighting(model,normal);
      for (int index : {0,1,2,0,2,3}) position(model,corners[index],light);
    };
    const auto box = [&](int kind,const Matrix& model,float x,float y,float z) {
      if (kind != material) return;
      // Beveled extrusion: broad matte faces and narrow rounded shoulders.
      // Narrow shoulders catch light without a painted outline.
      const float bevel=std::min(x*.28F,z*.55F);
      const std::array<Point,8> ring{{
        {-x+bevel,0,z},{x-bevel,0,z},{x,0,z-bevel},{x,0,-z+bevel},
        {x-bevel,0,-z},{-x+bevel,0,-z},{-x,0,-z+bevel},{-x,0,z-bevel}}};
      for (int face=0;face<8;++face) {
        const auto a=ring[face], b=ring[(face+1)%8];
        const float dx=b[0]-a[0], dz=b[2]-a[2];
        const float length=std::hypot(dx,dz);
        const Point normal{-dz/length,0,dx/length};
        quad(model,{{{a[0],-y,a[2]},{b[0],-y,b[2]},
                     {b[0],y,b[2]},{a[0],y,a[2]}}},normal);
        vertex(model,{0,y,0},{0,1,0});
        vertex(model,{a[0],y,a[2]},{0,1,0});
        vertex(model,{b[0],y,b[2]},{0,1,0});
        vertex(model,{0,-y,0},{0,-1,0});
        vertex(model,{b[0],-y,b[2]},{0,-1,0});
        vertex(model,{a[0],-y,a[2]},{0,-1,0});
      }
    };
    // The flexible rubber lip stays at the reveal edge while the frame flexes above it.
    box(0,translate(0,0,.004F),half*.74F,1.16F,.004F);
    box(0,translate(-half*.2F,0,.018F),half*.50F,1.15F,.012F);
    // Eight short sections follow the bowed pressure spine, with a narrow steel edge.
    for (int section=0; section<8; ++section) {
      const float a=-1.14F+section*.285F, b=a+.285F;
      const float y=(a+b)/2, z=(height(a)+height(b))/2;
      const float slope=std::atan2(height(b)-height(a),b-a);
      const float length=std::hypot(b-a,height(b)-height(a))/2;
      const Matrix pose=flex*translate(-half*.35F,y,z)*rotate_x(slope);
      box(1,pose,half*.50F,length,.012F);
      box(2,pose*translate(-half*.37F,0,.012F),half*.035F,length,.001F);
    }
    // Pressure clips and two articulated bridges are separate, visibly raised parts.
    for (float y : {-.86F,-.40F,.40F,.86F}) {
      box(1,flex*translate(0,y,.030F),half*.72F,.013F,.012F);
      box(2,flex*translate(-half*.1F,y,.050F),half*.12F,.010F,.003F);
    }
    const auto bridge = [&](float a,float b,float za,float zb) {
      const float length=std::hypot(b-a,zb-za)/2;
      const Matrix pose=flex*translate(-half*.70F,(a+b)/2,(za+zb)/2)*
                        rotate_x(std::atan2(zb-za,b-a));
      box(1,pose,half*.40F,length,.010F);
    };
    bridge(-.40F,0,.085F,.137F); bridge(0,.40F,.137F,.085F);
    bridge(-.86F,-.40F,.048F,.085F); bridge(.40F,.86F,.085F,.048F);
    box(1,flex*translate(-half*.4F,0,.137F),half*.83F,.043F,.016F);
    for (float side : {-1.0F,1.0F})
      box(1,flex*translate(-half*.4F+side*half*.67F,0,.132F)*rotate_y(side*.60F),
          half*.24F,.040F,.010F);
    // A round metal pivot pin gives the center coupling a mechanical silhouette.
    if (material == 2) {
      const Matrix pose=flex*translate(-half*.4F,0,.161F);
      const float radius=half*.18F;
      for (int side=0;side<12;++side) {
        const float a=2*pi*side/12, b=2*pi*(side+1)/12;
        const Point front_a{radius*std::cos(a),radius*std::sin(a),.004F};
        const Point front_b{radius*std::cos(b),radius*std::sin(b),.004F};
        vertex(pose,{0,0,.004F},{0,0,1});
        vertex(pose,front_a,{0,0,1}); vertex(pose,front_b,{0,0,1});
        quad(pose,{{front_a,{front_a[0],front_a[1],-.004F},
                    {front_b[0],front_b[1],-.004F},front_b}},
                    {std::cos((a+b)/2),std::sin((a+b)/2),0});
      }
    }
    mesh.materials[material]={first,mesh.count-first};
  }
}

}  // namespace pdfcube
