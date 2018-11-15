/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gles3jni.h"
#include <EGL/egl.h>
#include <vector>

#define STR(s) #s
#define STRV(s) STR(s)

struct Texture {
  Texture(int width, int height, int layers)
    : width(width), height(height)
  {
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, id);

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, width, height, layers);

    fbos.resize(layers);
    glGenFramebuffers(layers, fbos.data());
    for (int i = 0; i < layers; i++) {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[i]);
      glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER,
                                GL_COLOR_ATTACHMENT0,
                                id,
                                0,
                                i);
    }
  }

  GLuint id;
  int width;
  int height;
  std::vector<GLuint> fbos;
};

static void upload_rgb_layers(Texture* tex) {
    std::vector<unsigned char> buf(tex->width * tex->height * 4);

    // Upload red to layer 0
    for (int i = 0; i < buf.size(); i+=4) {
      buf[i]   = 0xFF;
      buf[i+1] = 0x00;
      buf[i+2] = 0x00;
      buf[i+3] = 0xFF;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex->id);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 0, tex->width, tex->height, 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

    // Upload green to layer 1
    for (int i = 0; i < buf.size(); i+=4) {
      buf[i]   = 0x00;
      buf[i+1] = 0xFF;
      buf[i+2] = 0x00;
      buf[i+3] = 0xFF;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex->id);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 1, tex->width, tex->height, 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

    // Upload blue to layer 2
    for (int i = 0; i < buf.size(); i+=4) {
      buf[i]   = 0x00;
      buf[i+1] = 0x00;
      buf[i+2] = 0xFF;
      buf[i+3] = 0xFF;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex->id);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 2, tex->width, tex->height, 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
}

static void blit_layers(Texture* src, Texture* dst) {
  for (int i = 0; i < src->fbos.size(); i++) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbos[i]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->fbos[i]);
    glBlitFramebuffer(0, 0, src->width, src->height,
                      0, 0, dst->width, dst->height,
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
  }
}

class RendererES3: public Renderer {
public:
    RendererES3();
    virtual ~RendererES3();
    bool init();

private:
    virtual void draw();

    const EGLContext mEglContext;

  Texture* src;
  Texture* dst;
};

Renderer* createES3Renderer() {
    RendererES3* renderer = new RendererES3;
    if (!renderer->init()) {
        delete renderer;
        return NULL;
    }
    return renderer;
}

RendererES3::RendererES3()
:   mEglContext(eglGetCurrentContext())
{
}

bool RendererES3::init() {
    ALOGV("Using OpenGL ES 3.0 renderer");

    src = new Texture(256, 256, 3);
    dst = new Texture(256, 256, 4);

    upload_rgb_layers(src);
    blit_layers(src, dst);

    return true;
}

RendererES3::~RendererES3() {
  if (eglGetCurrentContext() != mEglContext)
    return;

}

void RendererES3::draw() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    int y = 0;
    
    for (int i = 0; i < src->fbos.size(); i++) {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbos[i]);
      glBlitFramebuffer(0, 0, src->width, src->height,
                        i * src->width, y, (i + 1) * src->width, y + src->height,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    y += src->height;

    for (int i = 0; i < dst->fbos.size(); i++) {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, dst->fbos[i]);
      glBlitFramebuffer(0, 0, dst->width, dst->height,
                        i * dst->width, y, (i + 1) * dst->width, y + dst->height,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}
