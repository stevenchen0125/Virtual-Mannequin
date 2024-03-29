#include <GL/glew.h>
#include <debuggl.h>
#include <iostream>
#include "texture_to_render.h"

TextureToRender::TextureToRender()
{
}

TextureToRender::~TextureToRender()
{
	if (fb_ < 0)
		return ;
	unbind();
	glDeleteFramebuffers(1, &fb_);
	glDeleteTextures(1, &tex_);
	glDeleteRenderbuffers(1, &dep_);
}

void TextureToRender::create(int width, int height)
{
	w_ = width;
	h_ = height;
	// FIXME: Create the framebuffer object backed by a texture
	glGenFramebuffers(1, &fb_);
	glGenTextures(1, &tex_);
	glGenRenderbuffers(1, &dep_);
	bind();

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w_, h_, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w_, h_);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dep_);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex_, 0);
	GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, DrawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Failed to create framebuffer object as render target" << std::endl;
	} else {
		std::cerr << "Framebuffer ready" << std::endl;
	}

	unbind();
}

void TextureToRender::bind()
{
	// FIXME: Unbind the framebuffer object to GL_FRAMEBUFFER
	glBindFramebuffer(GL_FRAMEBUFFER, fb_);
	glBindTexture(GL_TEXTURE_2D, tex_);
	glBindRenderbuffer(GL_RENDERBUFFER, dep_);
}

void TextureToRender::unbind()
{
	// FIXME: Unbind current framebuffer object from the render target
	/*glDeleteFramebuffers(1, &fb_);
	glDeleteTextures(1, &tex_);
	glDeleteRenderbuffers(1, &dep_);*/
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}
