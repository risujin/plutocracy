/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* This file implements the PLUM (Plutocracy Model) model loading and rendering
   functions. */

#include "r_common.h"

/* Non-animated mesh */
typedef struct r_mesh {
        r_vbo_t vbo;
        r_vertex3_t *verts;
        unsigned short *indices;
        int verts_len, indices_len;
} mesh_t;

/* Model animation type */
typedef struct model_anim {
        int from, to, delay;
        char name[64], end_anim[64];
} model_anim_t;

/* Model object type */
typedef struct model_object {
        r_texture_t *texture;
        char name[64];
} model_object_t;

/* Animated, textured, multi-mesh model. The matrix contains enough room to
   store every object's static mesh for every frame, it is indexed by frame
   then by object. */
typedef struct r_model_data {
        c_ref_t ref;
        mesh_t *matrix;
        model_anim_t *anims;
        model_object_t *objects;
        int anims_len, objects_len, frames;
} model_data_t;

/* Linked list of loaded model data */
static c_ref_t *data_root;

/******************************************************************************\
 Render a mesh.
\******************************************************************************/
static void mesh_render(mesh_t *mesh)
{
        C_count_add(&r_count_faces, mesh->indices_len / 3);
        R_vbo_render(&mesh->vbo);

        /* Render the mesh normals for testing */
        R_render_normals(mesh->verts_len, &mesh->verts[0].co,
                         &mesh->verts[0].no, sizeof (*mesh->verts));
}

/******************************************************************************\
 Release the resources for a mesh.
\******************************************************************************/
static void mesh_cleanup(mesh_t *mesh)
{
        if (!mesh)
                return;
        C_free(mesh->verts);
        C_free(mesh->indices);
        R_vbo_cleanup(&mesh->vbo);
}

/******************************************************************************\
 Finish parsing an object and creates the mesh object. If Vertex Buffer Objects
 are supported, a new buffer is allocated and the vertex data is uploaded.
\******************************************************************************/
static int finish_object(model_data_t *data, int frame, int object,
                         c_array_t *verts, c_array_t *indices)
{
        mesh_t *mesh;
        int index, index_last;

        if (object < 0 || frame < 0)
                return TRUE;
        if (frame >= data->frames)
                C_error("Invalid frame %d", frame);
        if (object >= data->objects_len)
                C_error("Invalid obejct %d", object);
        mesh = data->matrix + frame * data->objects_len + object;
        mesh->verts_len = verts->len;
        mesh->verts = C_array_steal(verts);
        mesh->indices_len = indices->len;
        mesh->indices = C_array_steal(indices);
        index_last = (frame - 1) * data->objects_len + object;
        index = frame * data->objects_len + object;
        if (frame > 0 && data->matrix[index_last].indices_len !=
                         data->matrix[index].indices_len) {
                C_warning("PLUM file '%s' object '%s' faces mismatch at "
                          "frame %d", data->ref.name,
                          data->objects[object].name, frame);
                return FALSE;
        }

        /* Meshes are rendered through vertex buffer objects */
        R_vbo_init(&mesh->vbo, mesh->verts, mesh->verts_len,
                   sizeof (*mesh->verts), R_VERTEX3_FORMAT,
                   mesh->indices, mesh->indices_len);

        return TRUE;
}

/******************************************************************************\
 Free resources associated with the model private structure.
\******************************************************************************/
static void model_data_cleanup(model_data_t *data)
{
        int i;

        if (!data)
                return;
        if (data->matrix) {
                for (i = 0; i < data->objects_len * data->frames; i++)
                        mesh_cleanup(data->matrix + i);
                C_free(data->matrix);
        }
        for (i = 0; i < data->objects_len; i++)
                R_texture_free(data->objects[i].texture);
        C_free(data->objects);
        C_free(data->anims);
}

/******************************************************************************\
 Adds the [indices] given to the [array] if they are not culled. If they are,
 returns 1. Poorly-made models can contain faces that we can cull without
 losing much visual effect. This function checks if a face can be dropped.
\******************************************************************************/
static int add_face(const void *p, unsigned short indices[3], c_array_t *array,
                    bool cull)
{
        const r_vertex3_t *verts;
        c_vec3_t ab, ac, normal;
        float area;

        if (cull && r_model_lod.value.f > 0.f) {
                verts = (r_vertex3_t *)p;

                /* Cull faces that are too small */
                ab = C_vec3_sub(verts[indices[1]].co, verts[indices[0]].co);
                ac = C_vec3_sub(verts[indices[2]].co, verts[indices[0]].co);
                normal = C_vec3_cross(C_vec3_norm(ab), ac);
                area = C_vec3_len(normal);
                if (area < 0.02f * r_model_lod.value.f)
                        return 1;

                /* Cull faces that are downward-facing */
                normal = C_vec3_norm(normal);
                if (normal.y < -0.8f / r_model_lod.value.f)
                        return 1;
        }

        /* Face is good to be added */
        C_array_append(array, &indices[0]);
        C_array_append(array, &indices[1]);
        C_array_append(array, &indices[2]);

        return 0;
}

/******************************************************************************\
 Allocate memory for and load model data and its textures. Data is cached so
 calling this function again will return and reference the cached data.
\******************************************************************************/
static model_data_t *model_data_load(const char *filename, bool cull)
{
        c_token_file_t token_file;
        c_array_t anims, objects, verts, indices;
        model_data_t *data;
        const char *token;
        int found, quoted, object, frame, verts_parsed, faces_culled;

        if (!filename || !filename[0])
                return NULL;
        data = C_ref_alloc(sizeof (*data), &data_root,
                           (c_ref_cleanup_f)model_data_cleanup,
                           filename, &found);
        if (found)
                return data;

        /* Start parsing the file */
        C_zero(&verts);
        C_zero(&indices);
        if (!C_token_file_init(&token_file, filename)) {
                C_warning("Failed to open model '%s'", filename);
                goto error;
        }

        /* Load 'anims:' block */
        token = C_token_file_read(&token_file);
        if (!strcmp(token, "anims:")) {
                C_array_init(&anims, model_anim_t, 8);
                for (;;) {
                        model_anim_t anim;

                        /* Syntax: [name] [from] [to] [fps] [end-anim] */
                        token = C_token_file_read_full(&token_file, &quoted);
                        if (!quoted && !strcmp(token, "end"))
                                break;
                        C_strncpy(anim.name, token, sizeof (anim.name));
                        anim.from = atoi(C_token_file_read(&token_file)) - 1;
                        anim.to = atoi(C_token_file_read(&token_file)) - 1;
                        if (anim.from < 0 || anim.to < 0) {
                                C_warning("PLUM file '%s' contains invalid "
                                          "animation frame indices", filename);
                                goto error;
                        }
                        if (anim.from >= data->frames)
                                data->frames = anim.from + 1;
                        if (anim.to >= data->frames)
                                data->frames = anim.to + 1;
                        anim.delay = 1000 /
                                     atoi(C_token_file_read(&token_file));
                        if (anim.delay < 1)
                                anim.delay = 1;
                        token = C_token_file_read(&token_file);
                        C_strncpy(anim.end_anim, token, sizeof (anim.end_anim));
                        C_array_append(&anims, &anim);
                }
                data->anims_len = anims.len;
                data->anims = C_array_steal(&anims);
        } else {
                C_warning("PLUM file '%s' lacks anims block", filename);
                goto error;
        }

        /* Define objects */
        C_array_init(&objects, model_object_t, 8);
        for (;;) {
                model_object_t obj;

                token = C_token_file_read_full(&token_file, &quoted);
                if (strcmp(token, "object"))
                        break;
                token = C_token_file_read(&token_file);
                C_strncpy(obj.name, token, sizeof (obj.name));
                token = C_token_file_read(&token_file);
                obj.texture = R_texture_load(token, TRUE);
                C_array_append(&objects, &obj);
        }
        data->objects_len = objects.len;
        data->objects = C_array_steal(&objects);

        /* Load frames into matrix */
        faces_culled = 0;
        data->matrix = C_calloc(data->frames * data->objects_len *
                                sizeof (mesh_t));
        for (frame = -1, object = -1, verts_parsed = 0; token[0] || quoted;
             token = C_token_file_read_full(&token_file, &quoted)) {

                /* Each frame starts with 'frame #' where # is the frame
                   index numbered from 1 */
                if (!strcmp(token, "frame")) {
                        if (!finish_object(data, frame, object,
                                           &verts, &indices))
                                goto error;
                        object = -1;
                        if (++frame >= data->frames)
                                break;
                        token = C_token_file_read(&token_file);
                        if (atoi(token) != frame + 1) {
                                C_warning("PLUM file '%s' missing frames",
                                          filename);
                                goto error;
                        }
                        continue;
                }
                if (frame < 0) {
                        C_warning("PLUM file '%s' has '%s' instead of 'frame'",
                                  filename, token);
                        goto error;
                }

                /* Objects start with an 'o' and are listed in order */
                if (!strcmp(token, "o")) {
                        if (!finish_object(data, frame, object++,
                                           &verts, &indices))
                                goto error;
                        C_array_init(&verts, r_vertex3_t, 512);
                        C_array_init(&indices, unsigned short, 512);
                        verts_parsed = 0;
                        if (object > data->objects_len) {
                                C_warning("PLUM file '%s' frame %d has too"
                                          "many objects", filename, frame);
                                goto error;
                        }
                        continue;
                }
                if (object < 0) {
                        C_warning("PLUM file '%s' has '%s' instead of 'o'",
                                  filename, token);
                        goto error;
                }

                /* A 'v' means we need to add a new vertex */
                if (!strcmp(token, "v")) {
                        r_vertex3_t vert;

                        /* Syntax: v [coords x y z] [normal x y z] [uv x y] */
                        verts_parsed++;
                        vert.co.x = (float)atof(C_token_file_read(&token_file));
                        vert.co.y = (float)atof(C_token_file_read(&token_file));
                        vert.co.z = (float)atof(C_token_file_read(&token_file));
                        vert.no.x = (float)atof(C_token_file_read(&token_file));
                        vert.no.y = (float)atof(C_token_file_read(&token_file));
                        vert.no.z = (float)atof(C_token_file_read(&token_file));
                        vert.uv.x = (float)atof(C_token_file_read(&token_file));
                        vert.uv.y = (float)atof(C_token_file_read(&token_file));
                        C_array_append(&verts, &vert);

                        /* Parsing three new vertices automatically adds a
                           new face containing them */
                        if (verts_parsed >= 3) {
                                int i;
                                unsigned short face[3];

                                for (i = 0; i < 3; i++)
                                        face[i] = (unsigned short)
                                                  (verts.len - 3 + i);
                                faces_culled += add_face(verts.data, face,
                                                         &indices, cull);
                                verts_parsed = 0;
                        }
                        continue;
                }

                /* An 'i' means we can construct a new face using three
                   existing vertices */
                if (!strcmp(token, "i")) {
                        int i;
                        unsigned short face[3];

                        for (i = 0; i < 3; i++) {
                                token = C_token_file_read(&token_file);
                                face[i] = (unsigned short)atoi(token);
                                if (face[i] > verts.len) {
                                        C_warning("PLUM file '%s' contains "
                                                  "invalid index", filename);
                                        goto error;
                                }
                        }
                        faces_culled += add_face(verts.data, face,
                                                 &indices, cull);
                        verts_parsed = 0;
                        continue;
                }

                C_warning("PLUM file '%s' contains unrecognized token '%s'",
                          filename, token);
                goto error;
        }
        if (!finish_object(data, frame, object, &verts, &indices))
                goto error;
        if (frame < data->frames - 1) {
                C_warning("PLUM file '%s' lacks %d frame(s)",
                          filename, data->frames - frame + 1);
                goto error;
        }

        C_token_file_cleanup(&token_file);
        C_debug("Loaded '%s' (%d frm, %d obj, %d anim)",
                filename, data->frames, data->objects_len, data->anims_len);
        if (faces_culled > 0)
                C_debug("Culled %d faces total", faces_culled);
        return data;

error:  C_token_file_cleanup(&token_file);
        C_array_cleanup(&verts);
        C_array_cleanup(&indices);
        C_ref_down(&data->ref);
        return NULL;
}

/******************************************************************************\
 Updates animation progress and frame. Interpolates between key-frame meshes
 when necessary to smooth the animation.
\******************************************************************************/
static void update_animation(r_model_t *model)
{
        model_anim_t *anim;

        model->time_left -= c_frame_msec;
        if (model->time_left > 0)
                return;

        /* Advance a frame forward or backward */
        anim = model->data->anims + model->anim;
        model->last_frame = model->frame;
        if (anim->to > anim->from) {
                model->frame++;
                if (model->frame > anim->to)
                        R_model_play(model, anim->end_anim);
        } else {
                model->frame--;
                if (model->frame < anim->to)
                        R_model_play(model, anim->end_anim);
        }
        model->time_left = anim->delay;
        model->last_frame_time = c_time_msec;
}

/******************************************************************************\
 Render and advance the animation of a model. Applies the model's translation,
 rotation, and scale.

 This URL is helpful in demystifying the matrix operations:
 http://www.gamedev.net/reference/articles/article695.asp
\******************************************************************************/
void R_model_render(r_model_t *model)
{
        c_color_t add_color;
        c_vec3_t side;
        mesh_t *meshes;
        int i;

        if (!model || !model->data || model->modulate.a <= 0.f)
                return;
        R_push_mode(R_MODE_3D);

        /* Calculate the right-pointing vector. The forward and normal
           vectors had better be correct and normalized! */
        side = C_vec3_cross(model->normal, model->forward);

        /* X */
        model->matrix[0] = side.x * model->scale;
        model->matrix[4] = model->normal.x * model->scale;
        model->matrix[8] = model->forward.x * model->scale;
        model->matrix[12] = model->origin.x;

        /* Y */
        model->matrix[1] = side.y * model->scale;
        model->matrix[5] = model->normal.y * model->scale;
        model->matrix[9] = model->forward.y * model->scale;
        model->matrix[13] = model->origin.y;

        /* Z */
        model->matrix[2] = side.z * model->scale;
        model->matrix[6] = model->normal.z * model->scale;
        model->matrix[10] = model->forward.z * model->scale;
        model->matrix[14] = model->origin.z;

        /* W */
        model->matrix[3] = 0.f;
        model->matrix[7] = 0.f;
        model->matrix[11] = 0.f;
        model->matrix[15] = 1.f;

        glMultMatrixf(model->matrix);
        R_check_errors();

        /* Animate meshes. Interpolating here between frames is slow, it is best
           if models contain enough keyframes to maintain desired framerate. */
        if (model->time_left >= 0)
                update_animation(model);
        meshes = model->data->matrix +
                 model->data->objects_len * model->last_frame;

        /* We want to multisample models */
        if (r_multisample.value.n)
                R_gl_enable(GL_MULTISAMPLE);

        /* Unlit models need to temporarily disable lighting */
        if (model->unlit) {
                R_gl_disable(GL_LIGHTING);
                glColor4f(model->modulate.r, model->modulate.g,
                          model->modulate.b, model->modulate.a);
        }

        /* Lit models that are modulated need to set material colors */
        else {
                c_color_t color;

                color = C_color_scale(r_material[0], model->modulate);
                glMaterialfv(GL_FRONT, GL_AMBIENT, (GLfloat *)&color);
                color = C_color_scale(r_material[1], model->modulate);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, (GLfloat *)&color);
                color = C_color_scale(r_material[2], model->modulate);
                glMaterialfv(GL_FRONT, GL_SPECULAR, (GLfloat *)&color);
        }

        /* If this model is selected, multitexture the selected color */
        add_color = model->additive;
        if (model->selected == R_MS_SELECTED)
                add_color = C_color_add(model->additive, r_select_color);
        else if (model->selected == R_MS_HOVER)
                add_color = C_color_add(model->additive, r_hover_color);
        if (add_color.a > 0.f && r_white_tex && r_ext.multitexture > 1) {
                c_color_t mod_color;

                mod_color = C_color_mod(add_color, add_color.a);
                r_ext.glActiveTexture(GL_TEXTURE1);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, r_white_tex->gl_name);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
                glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
                glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
                           C_ARRAYF(mod_color));
                r_ext.glActiveTexture(GL_TEXTURE0);

                /* Render model meshes */
                for (i = 0; i < model->data->objects_len; i++) {
                        R_texture_select(model->data->objects[i].texture);
                        mesh_render(meshes + i);
                }

                glColor4f(1.f, 1.f, 1.f, 1.f);
                r_ext.glActiveTexture(GL_TEXTURE1);
                glDisable(GL_TEXTURE_2D);
                r_ext.glActiveTexture(GL_TEXTURE0);
        }

        /* Render model meshes normally */
        else
                for (i = 0; i < model->data->objects_len; i++) {
                        R_texture_select(model->data->objects[i].texture);
                        mesh_render(meshes + i);
                }

        R_gl_restore();
        R_pop_mode();
}

/******************************************************************************\
 Stop the model from playing animations.
\******************************************************************************/
static void model_stop(r_model_t *model)
{
        model->anim = 0;
        model->frame = 0;
        model->last_frame = 0;
        model->time_left = -1;
}

/******************************************************************************\
 Set a model animation to play.
\******************************************************************************/
void R_model_play(r_model_t *model, const char *name)
{
        int i;

        if (!model || !model->data)
                return;
        if (!name || !name[0]) {
                model_stop(model);
                return;
        }
        for (i = 0; i < model->data->anims_len; i++)
                if (!strcasecmp(model->data->anims[i].name, name)) {
                        model->anim = i;
                        model->frame = model->data->anims[i].from;
                        model->time_left = model->data->anims[i].delay;
                        model->last_frame_time = c_time_msec;
                        return;
                }
        model_stop(model);
        C_warning("Model '%s' lacks anim '%s'", model->data->ref.name, name);
}

static int model_traverse(r_model_t *self, visitproc visit, void *arg) {
        return 0;
}

/******************************************************************************\
 Clear model object
\******************************************************************************/
static int model_clear(r_model_t *self) {
        C_ref_down(&self->data->ref);
        return 0;
}

/******************************************************************************\
 Dealloc model object
\******************************************************************************/
static void model_dealloc(r_model_t* self) {
        model_clear(self);
        self->ob_type->tp_free((PyObject*)self);
}

/******************************************************************************\
 Alloc new model object
\******************************************************************************/
PyObject *model_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
        r_model_t *self;
        self = (r_model_t *)type->tp_alloc(type, 0);
        return (PyObject *)self;
}

/******************************************************************************\
 Initialise model object
\******************************************************************************/
int model_init(r_model_t *self , PyObject *args /*, PyObject *kwds */ )
{
        const char *filename;
        PyObject *cull;
        if(!PyArg_ParseTuple(args, "sO", &filename, &cull))
        {
                return -1;
        }
        self->data = model_data_load(filename, PyObject_IsTrue(cull));
        self->scale = 1.f;
        self->time_left = -1;
        self->normal = C_vec3(0.f, 1.f, 0.f);
        self->forward = C_vec3(0.f, 0.f, 1.f);
        self->modulate = C_color(1.f, 1.f, 1.f, 1.f);

        /* Start playing the first animation */
        if (self->data && self->data->anims_len)
                R_model_play(self, self->data->anims[0].name);
        return 0;
}

/******************************************************************************\
 Initialize a model instance. Model data is loaded if it is not already in
 memory. Returns FALSE and invalidates the model instance if the model data
 failed to load.
\******************************************************************************/
r_model_t *R_model_init(const char *filename, bool cull)
{
        r_model_t *model;
        PyObject *args;
        model = (r_model_t*)model_new(&R_model_type, NULL, NULL);
        args = Py_BuildValue("sO", filename, (cull) ? Py_True : Py_False);
        if(model_init(model, args))
                return NULL;
        return model;
}

/******************************************************************************\
 Model object members
\******************************************************************************/
static PyMemberDef model_members[] =
{
 {"selected", T_INT, offsetof(r_model_t, selected), 0, ""},
 {"anim", T_INT, offsetof(r_model_t, anim), 0, ""},
 {"frame", T_INT, offsetof(r_model_t, frame), 0, ""},
 {"last_frame", T_INT, offsetof(r_model_t, last_frame), 0, ""},
 {"last_frame_time", T_INT, offsetof(r_model_t, last_frame_time), 0, ""},
 {"time_left", T_INT, offsetof(r_model_t, time_left), 0, ""},
 {"scale", T_FLOAT, offsetof(r_model_t, scale), 0, ""},
 {NULL}  /* Sentinel */
};

/******************************************************************************\
 Model object methods
\******************************************************************************/
static PyMethodDef model_methods[] =
{
 {NULL}  /* Sentinel */
};

static PyGetSetDef model_getseters[] =
{
 {NULL}  /* Sentinel */
};

PyTypeObject R_model_type =
{
 PyObject_HEAD_INIT(NULL)
 0,                            /*ob_size*/
 "plutocracy.render.Model",    /*tp_name*/
 sizeof(r_model_t),            /*tp_basicsize*/
 0,                            /*tp_itemsize*/
 (destructor)model_dealloc,    /*tp_dealloc*/
 0,                            /*tp_print*/
 0,                            /*tp_getattr*/
 0,                            /*tp_setattr*/
 0,                            /*tp_compare*/
 0,                            /*tp_repr*/
 0,                            /*tp_as_number*/
 0,                            /*tp_as_sequence*/
 0,                            /*tp_as_mapping*/
 0,                            /*tp_hash */
 0,                            /*tp_call*/
 0,                            /*tp_str*/
 0,                            /*tp_getattro*/
 0,                            /*tp_setattro*/
 0,                            /*tp_as_buffer*/
 Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
 "Model",                      /* tp_doc */
 (traverseproc)model_traverse, /* tp_traverse */
 (inquiry)model_clear,         /* tp_clear */
 0,                            /* tp_richcompare */
 0,                            /* tp_weaklistoffset */
 0,                            /* tp_iter */
 0,                            /* tp_iternext */
 model_methods,                /* tp_methods */
 model_members,                /* tp_members */
 model_getseters,              /* tp_getset */
 0,                            /* tp_base */
 0,                            /* tp_dict */
 0,                            /* tp_descr_get */
 0,                            /* tp_descr_set */
 0,                            /* tp_dictoffset */
 (initproc)model_init,         /* tp_init */
 0,                            /* tp_alloc */
 model_new,                    /* tp_new */
};
