/*
** shader.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "shader.h"
#include "config.h"
#include "graphics.h"
#include "sharedstate.h"
#include "glstate.h"
#include "exception.h"

#include <assert.h>
#include <string.h>
#include <iostream>

#ifndef MKXPZ_BUILD_XCODE
#include "common.h.xxd"
#include "sprite.frag.xxd"
#include "hue.frag.xxd"
#include "trans.frag.xxd"
#include "transSimple.frag.xxd"
#include "bitmapBlit.frag.xxd"
#include "plane.frag.xxd"
#include "gray.frag.xxd"
#include "flatColor.frag.xxd"
#include "simple.frag.xxd"
#include "simpleColor.frag.xxd"
#include "simpleAlpha.frag.xxd"
#include "simpleAlphaUni.frag.xxd"
#include "tilemap.frag.xxd"
#include "flashMap.frag.xxd"
#include "bicubic.frag.xxd"
#include "lanczos3.frag.xxd"
#ifdef MKXPZ_SSL
#include "xbrz.frag.xxd"
#endif
#include "minimal.vert.xxd"
#include "simple.vert.xxd"
#include "simpleColor.vert.xxd"
#include "sprite.vert.xxd"
#include "tilemap.vert.xxd"
#include "blur.frag.xxd"
#include "simpleMatrix.vert.xxd"
#include "blurH.vert.xxd"
#include "blurV.vert.xxd"
#include "tilemapvx.vert.xxd"
#endif

#ifdef MKXPZ_BUILD_XCODE
#include "filesystem/filesystem.h"
#define INIT_SHADER(vert, frag, name) \
{ \
	std::string v = mkxp_fs::contentsOfAssetAsString("Shaders/" #vert, "vert"); \
	std::string f = mkxp_fs::contentsOfAssetAsString("Shaders/" #frag, "frag"); \
	Shader::init((const unsigned char *)v.c_str(), v.length(), (const unsigned char *)f.c_str(), f.length(), #vert, #frag, #name); \
}
#else
#define INIT_SHADER(vert, frag, name) \
{ \
	Shader::init(shader_##vert##_vert, shader_##vert##_vert_len, shader_##frag##_frag, shader_##frag##_frag_len, #vert, #frag, #name); \
}
#endif

#define GET_U(name) u_##name = gl.GetUniformLocation(program, #name)

#ifdef MKXPZ_BUILD_XCODE
std::string Shader::shaderCommon = "";
#endif

static void printShaderLog(GLuint shader)
{
	GLint logLength;
	gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

	std::string log(logLength, '\0');
	gl.GetShaderInfoLog(shader, log.size(), 0, &log[0]);

	std::clog << "Shader log:\n" << log;
}

static void printProgramLog(GLuint program)
{
	GLint logLength;
	gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

	std::string log(logLength, '\0');
	gl.GetProgramInfoLog(program, log.size(), 0, &log[0]);

	std::clog << "Program log:\n" << log;
}

Shader::Shader()
{
#ifdef MKXPZ_BUILD_XCODE
	if (Shader::shaderCommon.empty())
		Shader::shaderCommon = mkxp_fs::contentsOfAssetAsString("Shaders/common", "h");
#endif
	vertShader = gl.CreateShader(GL_VERTEX_SHADER);
	fragShader = gl.CreateShader(GL_FRAGMENT_SHADER);

	program = gl.CreateProgram();
}

Shader::~Shader()
{
	gl.DeleteProgram(program);
	gl.DeleteShader(vertShader);
	gl.DeleteShader(fragShader);
}

void Shader::bind()
{
	glState.program.set(program);
}

void Shader::unbind()
{
	gl.ActiveTexture(GL_TEXTURE0);
	glState.program.set(0);
}

#ifdef MKXPZ_BUILD_XCODE
std::string &Shader::commonHeader() {
	return Shader::shaderCommon;
}
#endif

static void setupShaderSource(GLuint shader, GLenum type, const unsigned char *body, int bodySize)
{
	static const char glesDefine[] = "#define GLSLES\n";
	static const char fragDefine[] = "#define FRAGMENT_SHADER\n";

	const GLchar *shaderSrc[4];
	GLint shaderSrcSize[4];
	size_t i = 0;

	if (gl.glsles)
	{
		shaderSrc[i] = glesDefine;
		shaderSrcSize[i] = sizeof(glesDefine)-1;
		++i;
	}

	if (type == GL_FRAGMENT_SHADER)
	{
		shaderSrc[i] = fragDefine;
		shaderSrcSize[i] = sizeof(fragDefine)-1;
		++i;
	}

#ifndef MKXPZ_BUILD_XCODE
	shaderSrc[i] = (const GLchar *) shader_common_h;
	shaderSrcSize[i] = shader_common_h_len;
#else
	shaderSrc[i] = (const GLchar *) Shader::commonHeader().c_str();
	shaderSrcSize[i] = Shader::commonHeader().length();
#endif
	++i;

	shaderSrc[i] = (const GLchar *) body;
	shaderSrcSize[i] = bodySize;
	++i;

	gl.ShaderSource(shader, i, shaderSrc, shaderSrcSize);
}

void Shader::init(const unsigned char *vert, int vertSize,
                  const unsigned char *frag, int fragSize,
                  const char *vertName, const char *fragName,
                  const char *programName)
{
	GLint success;

	/* Compile vertex shader */
	setupShaderSource(vertShader, GL_VERTEX_SHADER, vert, vertSize);
	gl.CompileShader(vertShader);

	gl.GetShaderiv(vertShader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		printShaderLog(vertShader);
		throw Exception(Exception::MKXPError,
	                    "GLSL: An error occured while compiling vertex shader '%s' in program '%s'",
	                    vertName, programName);
	}

	/* Compile fragment shader */
	setupShaderSource(fragShader, GL_FRAGMENT_SHADER, frag, fragSize);
	gl.CompileShader(fragShader);

	gl.GetShaderiv(fragShader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		printShaderLog(fragShader);
		throw Exception(Exception::MKXPError,
	                    "GLSL: An error occured while compiling fragment shader '%s' in program '%s'",
	                    fragName, programName);
	}

	/* Link shader program */
	gl.AttachShader(program, vertShader);
	gl.AttachShader(program, fragShader);

	gl.BindAttribLocation(program, Position, "position");
	gl.BindAttribLocation(program, TexCoord, "texCoord");
	gl.BindAttribLocation(program, Color, "color");

	gl.LinkProgram(program);

	gl.GetProgramiv(program, GL_LINK_STATUS, &success);

	if (!success)
	{
		printProgramLog(program);
		throw Exception(Exception::MKXPError,
	                    "GLSL: An error occured while linking program '%s' (vertex '%s', fragment '%s')",
	                    programName, vertName, fragName);
	}
}

void Shader::initFromFile(const char *_vertFile, const char *_fragFile,
                          const char *programName)
{
	std::string vertContents, fragContents;
	readFile(_vertFile, vertContents);
	readFile(_fragFile, fragContents);

	init((const unsigned char*) vertContents.c_str(), vertContents.size(),
	     (const unsigned char*) fragContents.c_str(), fragContents.size(),
	     _vertFile, _fragFile, programName);
}

void Shader::setVec2Uniform(GLint location, const Vec2 &vec)
{
    gl.Uniform2f(location, vec.x, vec.y);
}

void Shader::setVec4Uniform(GLint location, const Vec4 &vec)
{
	gl.Uniform4f(location, vec.x, vec.y, vec.z, vec.w);
}

void Shader::setTexUniform(GLint location, unsigned unitIndex, TEX::ID texture)
{
	GLenum texUnit = GL_TEXTURE0 + unitIndex;

	gl.ActiveTexture(texUnit);
	gl.BindTexture(GL_TEXTURE_2D, texture.gl);
	gl.Uniform1i(location, unitIndex);
	gl.ActiveTexture(GL_TEXTURE0);
}

void ShaderBase::GLProjMat::apply(const Vec2i &value)
{
	/* glOrtho replacement */
	const float a = 2.f / value.x;
	const float b = 2.f / value.y;
	const float c = -2.f;

	GLfloat mat[16] =
	{
		 a,  0,  0,  0,
		 0,  b,  0,  0,
		 0,  0,  c,  0,
		-1, -1, -1,  1
	};

	gl.UniformMatrix4fv(u_mat, 1, GL_FALSE, mat);
}

void ShaderBase::init()
{
	GET_U(texSizeInv);
	GET_U(translation);

	projMat.u_mat = gl.GetUniformLocation(program, "projMat");
}

void ShaderBase::applyViewportProj()
{
	// High-res: scale the matrix if we're rendering to the PingPong framebuffer.
	const IntRect &vp = glState.viewport.get();
	if (shState->config().enableHires && shState->graphics().isPingPongFramebufferActive() && framebufferScalingAllowed()) {
		projMat.set(Vec2i(shState->graphics().width(), shState->graphics().height()));
	}
	else {
		projMat.set(Vec2i(vp.w, vp.h));
	}
}

bool ShaderBase::framebufferScalingAllowed()
{
	return true;
}

void ShaderBase::setTexSize(const Vec2i &value)
{
	gl.Uniform2f(u_texSizeInv, 1.f / value.x, 1.f / value.y);
}

void ShaderBase::setTranslation(const Vec2i &value)
{
	gl.Uniform2f(u_translation, value.x, value.y);
}


FlatColorShader::FlatColorShader()
{
	INIT_SHADER(minimal, flatColor, FlatColorShader);

	ShaderBase::init();

	GET_U(color);
}

void FlatColorShader::setColor(const Vec4 &value)
{
	setVec4Uniform(u_color, value);
}


SimpleShader::SimpleShader()
{
	INIT_SHADER(simple, simple, SimpleShader);

	ShaderBase::init();

	GET_U(texOffsetX);
}

void SimpleShader::setTexOffsetX(int value)
{
	gl.Uniform1f(u_texOffsetX, value);
}


SimpleColorShader::SimpleColorShader()
{
	INIT_SHADER(simpleColor, simpleColor, SimpleColorShader);

	ShaderBase::init();
}


SimpleAlphaShader::SimpleAlphaShader()
{
	INIT_SHADER(simpleColor, simpleAlpha, SimpleAlphaShader);

	ShaderBase::init();
}


SimpleSpriteShader::SimpleSpriteShader()
{
	INIT_SHADER(sprite, simple, SimpleSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
}

void SimpleSpriteShader::setSpriteMat(const float value[16])
{
	gl.UniformMatrix4fv(u_spriteMat, 1, GL_FALSE, value);
}

BicubicSpriteShader::BicubicSpriteShader()
{
	INIT_SHADER(sprite, bicubic, BicubicSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(sourceSize);
	GET_U(bc);
}

void BicubicSpriteShader::setSharpness(int sharpness)
{
	gl.Uniform2f(u_bc, 1.f - sharpness * 0.01f, sharpness * 0.005f);
}

Lanczos3SpriteShader::Lanczos3SpriteShader()
{
	INIT_SHADER(sprite, lanczos3, Lanczos3SpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(sourceSize);
}

void Lanczos3SpriteShader::setTexSize(const Vec2i &value)
{
	ShaderBase::setTexSize(value);
	gl.Uniform2f(u_sourceSize, (float)value.x, (float)value.y);
}

#ifdef MKXPZ_SSL
XbrzSpriteShader::XbrzSpriteShader()
{
	INIT_SHADER(sprite, xbrz, XbrzSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(sourceSize);
	GET_U(targetScale);
}

void XbrzSpriteShader::setTargetScale(const Vec2 &value)
{
	gl.Uniform2f(u_targetScale, value.x, value.y);
}
#endif

AlphaSpriteShader::AlphaSpriteShader()
{
	INIT_SHADER(sprite, simpleAlphaUni, AlphaSpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(alpha);
}

void AlphaSpriteShader::setSpriteMat(const float value[16])
{
	gl.UniformMatrix4fv(u_spriteMat, 1, GL_FALSE, value);
}

void AlphaSpriteShader::setAlpha(float value)
{
	gl.Uniform1f(u_alpha, value);
}


TransShader::TransShader()
{
	INIT_SHADER(simple, trans, TransShader);

	ShaderBase::init();

	GET_U(currentScene);
	GET_U(frozenScene);
	GET_U(transMap);
	GET_U(prog);
	GET_U(vague);
}

void TransShader::setCurrentScene(TEX::ID tex)
{
	setTexUniform(u_currentScene, 1, tex);
}

void TransShader::setFrozenScene(TEX::ID tex)
{
	setTexUniform(u_frozenScene, 2, tex);
}

void TransShader::setTransMap(TEX::ID tex)
{
	setTexUniform(u_transMap, 3, tex);
}

void TransShader::setProg(float value)
{
	gl.Uniform1f(u_prog, value);
}

void TransShader::setVague(float value)
{
	gl.Uniform1f(u_vague, value);
}


SimpleTransShader::SimpleTransShader()
{
	INIT_SHADER(simple, transSimple, SimpleTransShader);

	ShaderBase::init();

	GET_U(currentScene);
	GET_U(frozenScene);
	GET_U(prog);
}

void SimpleTransShader::setCurrentScene(TEX::ID tex)
{
	setTexUniform(u_currentScene, 1, tex);
}

void SimpleTransShader::setFrozenScene(TEX::ID tex)
{
	setTexUniform(u_frozenScene, 2, tex);
}

void SimpleTransShader::setProg(float value)
{
	gl.Uniform1f(u_prog, value);
}


SpriteShader::SpriteShader()
{
	INIT_SHADER(sprite, sprite, SpriteShader);

	ShaderBase::init();

	GET_U(spriteMat);
	GET_U(tone);
	GET_U(color);
	GET_U(opacity);
	GET_U(bushDepth);
	GET_U(bushOpacity);
    GET_U(pattern);
    GET_U(patternBlendType);
    GET_U(patternTile);
    GET_U(renderPattern);
    GET_U(patternSizeInv);
    GET_U(patternOpacity);
    GET_U(patternScroll);
    GET_U(patternZoom);
    GET_U(invert);
}

void SpriteShader::setSpriteMat(const float value[16])
{
	gl.UniformMatrix4fv(u_spriteMat, 1, GL_FALSE, value);
}

void SpriteShader::setTone(const Vec4 &tone)
{
	setVec4Uniform(u_tone, tone);
}

void SpriteShader::setColor(const Vec4 &color)
{
	setVec4Uniform(u_color, color);
}

void SpriteShader::setOpacity(float value)
{
	gl.Uniform1f(u_opacity, value);
}

void SpriteShader::setBushDepth(float value)
{
	gl.Uniform1f(u_bushDepth, value);
}

void SpriteShader::setBushOpacity(float value)
{
	gl.Uniform1f(u_bushOpacity, value);
}

void SpriteShader::setPattern(const TEX::ID pattern, const Vec2 &dimensions)
{
    setTexUniform(u_pattern, 1, pattern);
    gl.Uniform2f(u_patternSizeInv, 1.f / dimensions.x, 1.f / dimensions.y);
}

void SpriteShader::setPatternBlendType(int blendType)
{
    gl.Uniform1i(u_patternBlendType, blendType);
}

void SpriteShader::setPatternTile(bool value)
{
    gl.Uniform1i(u_patternTile, value);
}

void SpriteShader::setShouldRenderPattern(bool value)
{
    gl.Uniform1i(u_renderPattern, value);
}

void SpriteShader::setPatternOpacity(float value)
{
    gl.Uniform1f(u_patternOpacity, value);
}

void SpriteShader::setPatternScroll(const Vec2 &scroll)
{
    setVec2Uniform(u_patternScroll, scroll);
}

void SpriteShader::setPatternZoom(const Vec2 &zoom)
{
    setVec2Uniform(u_patternZoom, zoom);
}

void SpriteShader::setInvert(bool value)
{
    gl.Uniform1i(u_invert, value);
}


PlaneShader::PlaneShader()
{
	INIT_SHADER(simple, plane, PlaneShader);

	ShaderBase::init();

	GET_U(tone);
	GET_U(color);
	GET_U(flash);
	GET_U(opacity);
}

void PlaneShader::setTone(const Vec4 &tone)
{
	setVec4Uniform(u_tone, tone);
}

void PlaneShader::setColor(const Vec4 &color)
{
	setVec4Uniform(u_color, color);
}

void PlaneShader::setFlash(const Vec4 &flash)
{
	setVec4Uniform(u_flash, flash);
}

void PlaneShader::setOpacity(float value)
{
	gl.Uniform1f(u_opacity, value);
}


GrayShader::GrayShader()
{
	INIT_SHADER(simple, gray, GrayShader);

	ShaderBase::init();

	GET_U(gray);
}

bool GrayShader::framebufferScalingAllowed()
{
	// This shader is used with input textures that have already had a
	// framebuffer scale applied. So we don't want to double-apply it.
	return false;
}

void GrayShader::setGray(float value)
{
	gl.Uniform1f(u_gray, value);
}


TilemapShader::TilemapShader()
{
	INIT_SHADER(tilemap, tilemap, TilemapShader);

	ShaderBase::init();

	GET_U(tone);
	GET_U(color);
	GET_U(opacity);

	GET_U(aniIndex);
	GET_U(atFrames);
}

void TilemapShader::setTone(const Vec4 &tone)
{
	setVec4Uniform(u_tone, tone);
}

void TilemapShader::setColor(const Vec4 &color)
{
	setVec4Uniform(u_color, color);
}

void TilemapShader::setOpacity(float value)
{
	gl.Uniform1f(u_opacity, value);
}

void TilemapShader::setAniIndex(int value)
{
	gl.Uniform1i(u_aniIndex, value);
}

void TilemapShader::setATFrames(int values[7])
{
	gl.Uniform1iv(u_atFrames, 7, values);
}



FlashMapShader::FlashMapShader()
{
	INIT_SHADER(simpleColor, flashMap, FlashMapShader);

	ShaderBase::init();

	GET_U(alpha);
}

void FlashMapShader::setAlpha(float value)
{
	gl.Uniform1f(u_alpha, value);
}


HueShader::HueShader()
{
	INIT_SHADER(simple, hue, HueShader);

	ShaderBase::init();

	GET_U(hueAdjust);
}

void HueShader::setHueAdjust(float value)
{
	gl.Uniform1f(u_hueAdjust, value);
}


SimpleMatrixShader::SimpleMatrixShader()
{
	INIT_SHADER(simpleMatrix, simpleAlpha, SimpleMatrixShader);

	ShaderBase::init();

	GET_U(matrix);
}

void SimpleMatrixShader::setMatrix(const float value[16])
{
	gl.UniformMatrix4fv(u_matrix, 1, GL_FALSE, value);
}


BlurShader::HPass::HPass()
{
	INIT_SHADER(blurH, blur, BlurShader::HPass);

	ShaderBase::init();
}

BlurShader::VPass::VPass()
{
	INIT_SHADER(blurV, blur, BlurShader::VPass);

	ShaderBase::init();
}


TilemapVXShader::TilemapVXShader()
{
	INIT_SHADER(tilemapvx, simple, TilemapVXShader);

	ShaderBase::init();

	GET_U(aniOffset);
}

void TilemapVXShader::setAniOffset(const Vec2 &value)
{
	gl.Uniform2f(u_aniOffset, value.x, value.y);
}


BltShader::BltShader()
{
	INIT_SHADER(simple, bitmapBlit, BltShader);

	ShaderBase::init();

	GET_U(source);
	GET_U(destination);
	GET_U(subRect);
	GET_U(opacity);
}

void BltShader::setSource()
{
	gl.Uniform1i(u_source, 0);
}

void BltShader::setDestination(const TEX::ID value)
{
	setTexUniform(u_destination, 1, value);
}

void BltShader::setSubRect(const FloatRect &value)
{
	gl.Uniform4f(u_subRect, value.x, value.y, value.w, value.h);
}

void BltShader::setOpacity(float value)
{
	gl.Uniform1f(u_opacity, value);
}

BicubicShader::BicubicShader()
{
	INIT_SHADER(simple, bicubic, BicubicShader);

	ShaderBase::init();

	GET_U(texOffsetX);
	GET_U(sourceSize);
	GET_U(bc);
}

void BicubicShader::setSharpness(int sharpness)
{
	gl.Uniform2f(u_bc, 1.f - sharpness * 0.01f, sharpness * 0.005f);
}

Lanczos3Shader::Lanczos3Shader()
{
	INIT_SHADER(simple, lanczos3, Lanczos3Shader);

	ShaderBase::init();

	GET_U(texOffsetX);
	GET_U(sourceSize);
}

void Lanczos3Shader::setTexSize(const Vec2i &value)
{
	ShaderBase::setTexSize(value);
	gl.Uniform2f(u_sourceSize, (float)value.x, (float)value.y);
}

#ifdef MKXPZ_SSL
XbrzShader::XbrzShader()
{
	INIT_SHADER(simple, xbrz, XbrzShader);

	ShaderBase::init();

	GET_U(texOffsetX);
	GET_U(sourceSize);
	GET_U(targetScale);
}

void XbrzShader::setTargetScale(const Vec2 &value)
{
	gl.Uniform2f(u_targetScale, value.x, value.y);
}
#endif
