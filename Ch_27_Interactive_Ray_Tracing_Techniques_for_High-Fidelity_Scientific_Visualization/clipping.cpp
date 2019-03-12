// This excerpt from Tachyon shows the simplicity with which
// one can implement a basic user-defined clipping plane feature
// (that globally clips all objects, when enabled) by storing clipping
// plane information in per-ray data and adding a simple distance
// comparison for each of the clipping plane(s) to be tested.

/* Only keeps closest intersection, no clipping, no CSG */
void add_regular_intersection(flt t, const object * obj, ray * ry) {
  if (t > EPSILON) {
    /* if we hit something before maxdist update maxdist */
    if (t < ry->maxdist) {
      ry->maxdist = t;
      ry->intstruct.num=1;
      ry->intstruct.closest.obj = obj;
      ry->intstruct.closest.t = t;
    }
  }
}

/* Only keeps closest intersection, also handles clipping, no CSG */
void add_clipped_intersection(flt t, const object * obj, ray * ry) {
  if (t > EPSILON) {
    /* if we hit something before maxdist update maxdist */
    if (t < ry->maxdist) {

      /* handle clipped object tests */
      if (obj->clip != NULL) {
        vector hit;
        int i;

        RAYPNT(hit, (*ry), t);    /* find the hit point for further tests */
        for (i=0; i<obj->clip->numplanes; i++) {
          if ((obj->clip->planes[i * 4    ] * hit.x +
               obj->clip->planes[i * 4 + 1] * hit.y +
               obj->clip->planes[i * 4 + 2] * hit.z) >
               obj->clip->planes[i * 4 + 3]) {
            return; /* hit point was clipped */
          }
        }
      }

      ry->maxdist = t;
      ry->intstruct.num=1;
      ry->intstruct.closest.obj = obj;
      ry->intstruct.closest.t = t;
    }
  }
}

/* Only meant for shadow rays, unsafe for anything else */
void add_shadow_intersection(flt t, const object * obj, ray * ry) {
  if (t > EPSILON) {
    /* if we hit something before maxdist update maxdist */
    if (t < ry->maxdist) {
      /* if this object doesn't cast a shadow, and we aren't  */
      /* limiting the number of transparent surfaces to less  */
      /* than 5, then modulate the light by its opacity value */
      if (!(obj->tex->flags & RT_TEXTURE_SHADOWCAST)) {
        if (ry->scene->shadowfilter)
          ry->intstruct.shadowfilter *= (1.0 - obj->tex->opacity);
        return;
      }

      ry->maxdist = t;
      ry->intstruct.num=1;

      /* if we hit *anything* before maxdist, and we're firing a */
      /* shadow ray, then we are finished ray tracing the shadow */
      ry->flags |= RT_RAY_FINISHED;
    }
  }
}

/* Only meant for clipped shadow rays, unsafe for anything else */
void add_clipped_shadow_intersection(flt t, const object * obj, ray * ry) {
  if (t > EPSILON) {
    /* if we hit something before maxdist update maxdist */
    if (t < ry->maxdist) {
      /* if this object doesn't cast a shadow, and we aren't  */
      /* limiting the number of transparent surfaces to less  */
      /* than 5, then modulate the light by its opacity value */
      if (!(obj->tex->flags & RT_TEXTURE_SHADOWCAST)) {
        if (ry->scene->shadowfilter)
          ry->intstruct.shadowfilter *= (1.0 - obj->tex->opacity);
        return;
      }

      /* handle clipped object tests */
      if (obj->clip != NULL) {
        vector hit;
        int i;

        RAYPNT(hit, (*ry), t);    /* find the hit point for further tests */
        for (i=0; i<obj->clip->numplanes; i++) {
          if ((obj->clip->planes[i * 4    ] * hit.x +
               obj->clip->planes[i * 4 + 1] * hit.y +
               obj->clip->planes[i * 4 + 2] * hit.z) >
               obj->clip->planes[i * 4 + 3]) {
            return; /* hit point was clipped */
          }
        }
      }

      ry->maxdist = t;
      ry->intstruct.num=1;

      /* if we hit *anything* before maxdist, and we're firing a */
      /* shadow ray, then we are finished ray tracing the shadow */
      ry->flags |= RT_RAY_FINISHED;
    }
  }
}
