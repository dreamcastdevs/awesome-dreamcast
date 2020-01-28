/*
 * mdl.c -- mdl model loader
 * last modification: mar. 21, 2015
 *
 * Copyright (c) 2005-2015 David HENRY
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * gcc -Wall -ansi -lGL -lGLU -lglut mdl.c -o mdl
 */

#include "mdl.h"

#include "anorms.h"
#include "colormap_lmp.h"

static int num_frame_verts;

/*** An MDL model ***/
struct mdl_model_t mdlfile;

/**
 * Make a texture given a skin index 'n'.
 */
GLuint
MakeTextureFromSkin(int n, const struct mdl_model_t *mdl)
{
    int i;
    GLuint id;

    GLubyte *pixels = (GLubyte *)
        malloc(mdl->header.skinwidth * mdl->header.skinheight * 3);

    /* Convert indexed 8 bits texture to RGB 24 bits */
    for (i = 0; i < mdl->header.skinwidth * mdl->header.skinheight; ++i)
    {
        pixels[(i * 3) + 0] = colormap[mdl->skins[n].data[i]][0];
        pixels[(i * 3) + 1] = colormap[mdl->skins[n].data[i]][1];
        pixels[(i * 3) + 2] = colormap[mdl->skins[n].data[i]][2];
    }

    /* Generate OpenGL texture */
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    /*gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, mdl->header.skinwidth,
                      mdl->header.skinheight, GL_RGB, GL_UNSIGNED_BYTE,
                      pixels);
    */
    printf("w: %d, h: %d\n", mdl->header.skinwidth, mdl->header.skinheight);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mdl->header.skinwidth, mdl->header.skinheight, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    /* OpenGL has its own copy of image data */
    free(pixels);
    return id;
}

/**
 * Load an MDL model from file.
 *
 * Note: MDL format stores model's data in little-endian ordering.  On
 * big-endian machines, you'll have to perform proper conversions.
 */
int ReadMDLModel(const char *filename, struct mdl_model_t *mdl)
{
    FILE *fp;
    int i;

    fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "error: couldn't open \"%s\"!\n", filename);
        return 0;
    }

    /* Read header */
    fread(&mdl->header, 1, sizeof(struct mdl_header_t), fp);

    if ((mdl->header.ident != 1330660425) ||
        (mdl->header.version != 6))
    {
        /* Error! */
        fprintf(stderr, "Error: bad version or identifier\n");
        fclose(fp);
        return 0;
    }

    /* Memory allocations */
    mdl->skins = (struct mdl_skin_t *)
        malloc(sizeof(struct mdl_skin_t) * mdl->header.num_skins);
    mdl->texcoords = (struct mdl_texcoord_t *)
        malloc(sizeof(struct mdl_texcoord_t) * mdl->header.num_verts);
    mdl->triangles = (struct mdl_triangle_t *)
        malloc(sizeof(struct mdl_triangle_t) * mdl->header.num_tris);
    mdl->frames = (struct mdl_frame_t *)
        malloc(sizeof(struct mdl_frame_t) * mdl->header.num_frames);
    mdl->tex_id = (GLuint *)
        malloc(sizeof(GLuint) * mdl->header.num_skins);

    mdl->iskin = 0;

    /* Read texture data */
    for (i = 0; i < mdl->header.num_skins; ++i)
    {
        mdl->skins[i].data = (GLubyte *)malloc(sizeof(GLubyte) * mdl->header.skinwidth * mdl->header.skinheight);

        fread(&mdl->skins[i].group, sizeof(int), 1, fp);
        fread(mdl->skins[i].data, sizeof(GLubyte),
              mdl->header.skinwidth * mdl->header.skinheight, fp);

        mdl->tex_id[i] = MakeTextureFromSkin(i, mdl);

        free(mdl->skins[i].data);
        mdl->skins[i].data = NULL;
    }

    fread(mdl->texcoords, sizeof(struct mdl_texcoord_t),
          mdl->header.num_verts, fp);
    fread(mdl->triangles, sizeof(struct mdl_triangle_t),
          mdl->header.num_tris, fp);

    /* Read frames */
    for (i = 0; i < mdl->header.num_frames; ++i)
    {
        /* Memory allocation for vertices of this frame */
        mdl->frames[i].frame.verts = (struct mdl_vertex_t *)
            malloc(sizeof(struct mdl_vertex_t) * mdl->header.num_verts);

        /* Read frame data */
        fread(&mdl->frames[i].type, sizeof(int), 1, fp);
        fread(&mdl->frames[i].frame.bboxmin,
              sizeof(struct mdl_vertex_t), 1, fp);
        fread(&mdl->frames[i].frame.bboxmax,
              sizeof(struct mdl_vertex_t), 1, fp);
        fread(mdl->frames[i].frame.name, sizeof(char), 16, fp);
        fread(mdl->frames[i].frame.verts, sizeof(struct mdl_vertex_t),
              mdl->header.num_verts, fp);
    }

    fclose(fp);
    return 1;
}

/**
 * Free resources allocated for the model.
 */
void FreeModel(struct mdl_model_t *mdl)
{
    int i;

    if (mdl->skins)
    {
        free(mdl->skins);
        mdl->skins = NULL;
    }

    if (mdl->texcoords)
    {
        free(mdl->texcoords);
        mdl->texcoords = NULL;
    }

    if (mdl->triangles)
    {
        free(mdl->triangles);
        mdl->triangles = NULL;
    }

    if (mdl->tex_id)
    {
        /* Delete OpenGL textures */
        glDeleteTextures(mdl->header.num_skins, mdl->tex_id);

        free(mdl->tex_id);
        mdl->tex_id = NULL;
    }

    if (mdl->frames)
    {
        for (i = 0; i < mdl->header.num_frames; ++i)
        {
            free(mdl->frames[i].frame.verts);
            mdl->frames[i].frame.verts = NULL;
        }

        free(mdl->frames);
        mdl->frames = NULL;
    }
}

/**
 * Render the model at frame n.
 */
void RenderFrame(int n, const struct mdl_model_t *mdl)
{
    int i, j;
    GLfloat s, t;
    vec3 v;
    struct mdl_vertex_t *pvert;

    /* Check if n is in a valid range */
    if ((n < 0) || (n > mdl->header.num_frames - 1))
        return;

    /* Enable model's texture */
    glBindTexture(GL_TEXTURE_2D, mdl->tex_id[mdl->iskin]);

    /* Draw the model */
    glBegin(GL_TRIANGLES);

    /* Draw each triangle */
    for (i = 0; i < mdl->header.num_tris; ++i)
    {
        /* Draw each vertex */
        for (j = 0; j < 3; ++j)
        {
            pvert = &mdl->frames[n].frame.verts[mdl->triangles[i].vertex[j]];

            /* Compute texture coordinates */
            s = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].s;
            t = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].t;

            if (!mdl->triangles[i].facesfront &&
                mdl->texcoords[mdl->triangles[i].vertex[j]].onseam)
            {
                s += mdl->header.skinwidth * 0.5f; /* Backface */
            }

            /* Scale s and t to range from 0.0 to 1.0 */
            s = (s + 0.5) / mdl->header.skinwidth;
            t = (t + 0.5) / mdl->header.skinheight;

            /* Pass texture coordinates to OpenGL */
            glTexCoord2f(s, t);

            /* Normal vector */
            //glNormal3fv(anorms[pvert->normalIndex]);

            /* Calculate real vertex position */
            v[0] = (mdl->header.scale[0] * pvert->v[0]) + mdl->header.translate[0];
            v[1] = (mdl->header.scale[1] * pvert->v[1]) + mdl->header.translate[1];
            v[2] = (mdl->header.scale[2] * pvert->v[2]) + mdl->header.translate[2];

            glVertex3fv(v);
        }
    }
    glEnd();
}

/**
 * Render the model with interpolation between frame n and n+1.
 * interp is the interpolation percent. (from 0.0 to 1.0)
 */
void RenderFrameItp_Immediate(int n, float interp, const struct mdl_model_t *mdl)
{
    int i, j;
    GLfloat s, t;
    //vec3 norm, v;
    vec3 v;
    //GLfloat *n_curr, *n_next;
    struct mdl_vertex_t *pvert1, *pvert2;

    /* Check if n is in a valid range */
    if ((n < 0) || (n > mdl->header.num_frames))
        return;

    /* Enable model's texture */
    glBindTexture(GL_TEXTURE_2D, mdl->tex_id[mdl->iskin]);

    /* Draw the model */
    glBegin(GL_TRIANGLES);
    /* Draw each triangle */
    for (i = 0; i < mdl->header.num_tris; ++i)
    {
        /* Draw each vertex */
        for (j = 0; j < 3; ++j)
        {
            pvert1 = &mdl->frames[n].frame.verts[mdl->triangles[i].vertex[j]];
            pvert2 = &mdl->frames[n + 1].frame.verts[mdl->triangles[i].vertex[j]];

            /* Compute texture coordinates */
            s = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].s;
            t = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].t;

            if (!mdl->triangles[i].facesfront &&
                mdl->texcoords[mdl->triangles[i].vertex[j]].onseam)
            {
                s += mdl->header.skinwidth * 0.5f; /* Backface */
            }

            /* Scale s and t to range from 0.0 to 1.0 */
            s = (s + 0.5) / mdl->header.skinwidth;
            t = (t + 0.5) / mdl->header.skinheight;

            /* Pass texture coordinates to OpenGL */
            glTexCoord2f(s, t);

            /* Interpolate normals */
            /*
            n_curr = anorms[pvert1->normalIndex];
            n_next = anorms[pvert2->normalIndex];

            norm[0] = n_curr[0] + interp * (n_next[0] - n_curr[0]);
            norm[1] = n_curr[1] + interp * (n_next[1] - n_curr[1]);
            norm[2] = n_curr[2] + interp * (n_next[2] - n_curr[2]);

            glNormal3fv(norm);
            */

            /* Interpolate vertices */
            v[0] = mdl->header.scale[0] * (pvert1->v[0] + interp * (pvert2->v[0] - pvert1->v[0])) + mdl->header.translate[0];
            v[1] = mdl->header.scale[1] * (pvert1->v[1] + interp * (pvert2->v[1] - pvert1->v[1])) + mdl->header.translate[1];
            v[2] = mdl->header.scale[2] * (pvert1->v[2] + interp * (pvert2->v[2] - pvert1->v[2])) + mdl->header.translate[2];

            glVertex3fv(v);
        }
    }
    glEnd();

    num_frame_verts += (mdl->header.num_tris * 3);
}

void RenderFrameItp_Array(int n, float interp, const struct mdl_model_t *mdl)
{
    int i, j;
    GLfloat s, t;
    //vec3 norm, v;
    //vec3 v;
    //GLfloat *n_curr, *n_next;
    struct mdl_vertex_t *pvert1, *pvert2;

    /* Check if n is in a valid range */
    if ((n < 0) || (n > mdl->header.num_frames))
        return;

    /* Enable model's texture */
    glBindTexture(GL_TEXTURE_2D, mdl->tex_id[mdl->iskin]);

    vec3 tris[mdl->header.num_tris * 3];
    vec2 texcoords[mdl->header.num_tris * 3];

    glDisableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glPushMatrix();
    glVertexPointer(3, GL_FLOAT, 0, tris);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

    //glBegin(GL_TRIANGLES);
    /* Draw each triangle */
    for (i = 0; i < mdl->header.num_tris; ++i)
    {
        /* Draw each vertex */
        for (j = 0; j < 3; ++j)
        {
            pvert1 = &mdl->frames[n].frame.verts[mdl->triangles[i].vertex[j]];
            pvert2 = &mdl->frames[n + 1].frame.verts[mdl->triangles[i].vertex[j]];

            /* Compute texture coordinates */
            s = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].s;
            t = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].t;

            if (!mdl->triangles[i].facesfront &&
                mdl->texcoords[mdl->triangles[i].vertex[j]].onseam)
            {
                s += mdl->header.skinwidth * 0.5f; /* Backface */
            }

            /* Scale s and t to range from 0.0 to 1.0 */
            s = (s + 0.5) / mdl->header.skinwidth;
            t = (t + 0.5) / mdl->header.skinheight;
            texcoords[(i * 3) + j][0] = s;
            texcoords[(i * 3) + j][1] = t;

            /* Pass texture coordinates to OpenGL */
            //glTexCoord2f(s, t);

            /* Interpolate normals */
            /*
            n_curr = anorms[pvert1->normalIndex];
            n_next = anorms[pvert2->normalIndex];

            norm[0] = n_curr[0] + interp * (n_next[0] - n_curr[0]);
            norm[1] = n_curr[1] + interp * (n_next[1] - n_curr[1]);
            norm[2] = n_curr[2] + interp * (n_next[2] - n_curr[2]);

            glNormal3fv(norm);
            */
            float *v = (float *)&tris[(i * 3) + j];

            /* Interpolate vertices */
            v[0] = mdl->header.scale[0] * (pvert1->v[0] + interp * (pvert2->v[0] - pvert1->v[0])) + mdl->header.translate[0];
            v[1] = mdl->header.scale[1] * (pvert1->v[1] + interp * (pvert2->v[1] - pvert1->v[1])) + mdl->header.translate[1];
            v[2] = mdl->header.scale[2] * (pvert1->v[2] + interp * (pvert2->v[2] - pvert1->v[2])) + mdl->header.translate[2];

            //glVertex3fv(v);
        }
    }
    //glEnd();
    glDrawArrays(GL_TRIANGLES, 0, mdl->header.num_tris * 3);
    glPopMatrix();

    num_frame_verts += (mdl->header.num_tris * 3);
}

static glvert_fast_t gVertexFastBuffer[512 * 3];
static int r_numsurfvertexes;
void RenderFrameItp_fast_vert(int n, float interp, const struct mdl_model_t *mdl)
{
    int i, j;
    GLfloat s, t;
    //vec3 norm, v;
    //vec3 v;
    //GLfloat *n_curr, *n_next;
    struct mdl_vertex_t *pvert1, *pvert2;
    r_numsurfvertexes = 0;

    /* Check if n is in a valid range */
    if ((n < 0) || (n > mdl->header.num_frames))
        return;

    /* Enable model's texture */
    glBindTexture(GL_TEXTURE_2D, mdl->tex_id[mdl->iskin]);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glPushMatrix();
    glVertexPointer(3, GL_FLOAT, sizeof(glvert_fast_t), &gVertexFastBuffer[0].vert);
    glTexCoordPointer(2, GL_FLOAT, sizeof(glvert_fast_t), &gVertexFastBuffer[0].texture);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(glvert_fast_t), &gVertexFastBuffer[0].color);

    //glBegin(GL_TRIANGLES);
    /* Draw each triangle */
    for (i = 0; i < mdl->header.num_tris; ++i)
    {
        /* Draw each vertex */
        for (j = 0; j < 3; ++j)
        {
            pvert1 = &mdl->frames[n].frame.verts[mdl->triangles[i].vertex[j]];
            pvert2 = &mdl->frames[n + 1].frame.verts[mdl->triangles[i].vertex[j]];

            /* Compute texture coordinates */
            s = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].s;
            t = (GLfloat)mdl->texcoords[mdl->triangles[i].vertex[j]].t;

            if (!mdl->triangles[i].facesfront &&
                mdl->texcoords[mdl->triangles[i].vertex[j]].onseam)
            {
                s += mdl->header.skinwidth * 0.5f; /* Backface */
            }

            /* Scale s and t to range from 0.0 to 1.0 */
            s = (s + 0.5) / mdl->header.skinwidth;
            t = (t + 0.5) / mdl->header.skinheight;

            /* Pass texture coordinates to OpenGL */
            //glTexCoord2f(s, t);

            /* Interpolate normals */
            /*
            n_curr = anorms[pvert1->normalIndex];
            n_next = anorms[pvert2->normalIndex];

            norm[0] = n_curr[0] + interp * (n_next[0] - n_curr[0]);
            norm[1] = n_curr[1] + interp * (n_next[1] - n_curr[1]);
            norm[2] = n_curr[2] + interp * (n_next[2] - n_curr[2]);

            glNormal3fv(norm);
            */
            float v[3];

            /* Interpolate vertices */
            v[0] = mdl->header.scale[0] * (pvert1->v[0] + interp * (pvert2->v[0] - pvert1->v[0])) + mdl->header.translate[0];
            v[1] = mdl->header.scale[1] * (pvert1->v[1] + interp * (pvert2->v[1] - pvert1->v[1])) + mdl->header.translate[1];
            v[2] = mdl->header.scale[2] * (pvert1->v[2] + interp * (pvert2->v[2] - pvert1->v[2])) + mdl->header.translate[2];
            gVertexFastBuffer[r_numsurfvertexes++] = (glvert_fast_t){.flags = (j == 2) ? VERTEX_EOL : VERTEX, .vert = {v[0], v[1], v[2]}, .texture = {s, t}, .color = {255, 255, 255, 255}, .pad0 = {0}};

            //glVertex3fv(v);
        }
    }
    //glEnd();
    glDrawArrays(GL_TRIANGLES, 0, r_numsurfvertexes);
    glPopMatrix();

    glDisableClientState(GL_COLOR_ARRAY);
    num_frame_verts += r_numsurfvertexes;
    r_numsurfvertexes = 0;
}

/**
 * Calculate the current frame in animation beginning at frame
 * 'start' and ending at frame 'end', given interpolation percent.
 * interp will be reseted to 0.0 if the next frame is reached.
 */
void Animate(int start, int end, int *frame, float *interp)
{
    if ((*frame < start) || (*frame > end))
        *frame = start;

    if (*interp >= 1.0f)
    {
        /* Move to next frame */
        *interp = 0.0f;
        (*frame)++;

        if (*frame >= end)
            *frame = start;
    }
}

void loadMDL(const char *filename)
{
    //GLfloat lightpos[] = {5.0f, 10.0f, 0.0f, 1.0f};

    /* Initialize OpenGL context */
    glShadeModel(GL_SMOOTH);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    //glEnable(GL_LIGHTING);
    //glEnable(GL_LIGHT0);

    //glLightfv(GL_LIGHT0, GL_POSITION, lightpos);

    /* Load MDL model file */
    /*if (!ReadMDLModel(filename, &mdlfile))
        exit(EXIT_FAILURE);
    */
    ReadMDLModel(filename, &mdlfile);
}

void cleanup(void)
{
    FreeModel(&mdlfile);
}

int number_verts(void)
{
    return num_frame_verts;
}

void display(int mode, int amount)
{
    num_frame_verts = 0;
    glDisable(GL_CULL_FACE);
    static int n = 0;
    static float interp = 0.0;
    static double curent_time = 0;
    static double last_time = 0;

    last_time = curent_time;
    curent_time = Sys_FloatTime();

    /* Animate model from frames 0 to num_frames-1 */
    interp += 10 * (curent_time - last_time);
    Animate(0, mdlfile.header.num_frames - 1, &n, &interp);
    for (int i = 0; i <= amount; i++)
    {
        glPushMatrix();
        glTranslatef(-20.0f + (20 * (i % 3)), 15.0f - (14 * (i / 3)), -60.0f);
        glRotatef(-90.0f, 1.0, 0.0, 0.0);
        glRotatef(-90.0f, 0.0, 0.0, 1.0);
        glScalef(0.25f, 0.25f, 0.25f);

        switch (mode)
        {
        case 0:
            /* Draw the model using Immediate Mode GL */
            if (mdlfile.header.num_frames > 1)
                RenderFrameItp_Immediate(n, interp, &mdlfile);
            else
                RenderFrame(n, &mdlfile);
            break;
        case 1:
            /* Draw the model using GL Vertex Arrays */
            if (mdlfile.header.num_frames > 1)
                RenderFrameItp_Array(n, interp, &mdlfile);
            //else
            //    RenderFrame(n, &mdlfile);
            break;
        case 2:
            /* Draw the model using gl_fast_verts */
            if (mdlfile.header.num_frames > 1)
                RenderFrameItp_fast_vert(n, interp, &mdlfile);
            //else
            //    RenderFrame(n, &mdlfile);
            break;
            break;
        }
        glPopMatrix();
    }

#if 0
    glTranslatef(0.0f, 0.0f, -50.0f);
    glRotatef(-90.0f, 1.0, 0.0, 0.0);
    glRotatef(-90.0f, 0.0, 0.0, 1.0);
    glScalef(0.5f, 0.5f, 0.5f);

    switch (mode)
    {
    case 0:
        /* Draw the model using Immediate Mode GL */
        if (mdlfile.header.num_frames > 1)
            RenderFrameItp_Immediate(n, interp, &mdlfile);
        else
            RenderFrame(n, &mdlfile);
        break;
    case 1:
        /* Draw the model using GL Vertex Arrays */
        if (mdlfile.header.num_frames > 1)
            RenderFrameItp_Array(n, interp, &mdlfile);
        //else
        //    RenderFrame(n, &mdlfile);
        break;
    case 2:
        break;
    }
#endif
}
