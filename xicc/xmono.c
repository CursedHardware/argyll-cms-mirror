/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    2/7/00
 * Version: 1.00
 *
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the old iccXfm class.
 */

/*
 * This module provides the expands icclib functionality
 * for monochrome profiles.
 * This file is #included in xicc.c, to keep its functions private.
 */

/*
 * TTBD:
 *       Some of the error handling is crude. Shouldn't use
 *       error(), should return status.
 *
 */

/* ============================================================= */
/* Forward and Backward Monochrome type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* - - - - - - - - - - - - - - - - - - - - - */
/* Individual components of Fwd conversion: */
/* Because icm_lu4 sets up component conversions in the requested direction, */
/* but icxLuMonox expects component conversions as if the the matrix is */
/* fwd, we have if (dir) code to reverse things here. */
static int
icxLuMonoFwd_curve (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icmPe_lurv prv;
	if (p->dir) {
		prv =  p->plu->output_fmt_bwd(p->plu, out, in);	/* Should be NOP */
		prv |= p->plu->output_pch_bwd(p->plu, out, out);
	} else {
		prv =  p->plu->input_fmt_fwd(p->plu, out, in);	/* Should be NOP */
		prv |= p->plu->input_pch_fwd(p->plu, out, out);
	}
	return LUE2XLUE(prv);
}

static int
icxLuMonoFwd_map (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icmPe_lurv prv;
	if (p->dir) {
		prv = p->plu->core5_bwd(p->plu, out, in);
	} else {
		prv = p->plu->core5_fwd(p->plu, out, in);
	}
	return LUE2XLUE(prv);
}

static int
icxLuMonoFwd_abs (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmPe_lurv prv;
	if (p->dir) {
		prv = p->plu->input_pch_bwd(p->plu, out, in);		/* Should be NOP */
		prv |= p->plu->input_fmt_bwd(p->plu, out, out);
	} else {
		prv = p->plu->output_pch_fwd(p->plu, out, in);		/* Should be NOP */
		prv |= p->plu->output_fmt_fwd(p->plu, out, out);
	}
	rv = LUE2XLUE(prv);

	if (p->pcs == icxSigJabData) {
		p->cam->XYZ_to_cam(p->cam, out, out);
	}
	return rv;
}


/* Overall Fwd conversion routine */
/* Note that the overall conversion is in the requested direction, */
/* as the setup code swaps icxLuMonoxFwd_lookup/icxLuMonoxBwd_lookup */
/* as needed. */
static int
icxLuMonoFwd_lookup (
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icxLuMono *p = (icxLuMono *)pp;
	rv |= icxLuMonoFwd_curve(p, out, in);
	rv |= icxLuMonoFwd_map(p, out, out);
	rv |= icxLuMonoFwd_abs(p, out, out);
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given a relative XYZ or Lab PCS value, convert in the fwd direction into */ 
/* the nominated output PCS (ie. Absolute, Jab etc.) */
/* (This is used in generating gamut compression in B2A tables) */
void icxLuMono_fwd_relpcs_outpcs(
icxLuBase *pp,
icColorSpaceSignature is,		/* Input space, XYZ or Lab */
double *out, double *in) {
	icxLuMono *p = (icxLuMono *)pp;

	icmLab2XYZ(&icmD50, out, in);
	if (is == icSigLabData && p->natpcs == icSigXYZData) {
		icxLuMonoFwd_abs(p, out, out);
	} else if (is == icSigXYZData && p->natpcs == icSigLabData) {
		icxLuMonoFwd_abs(p, out, out);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - */
/* Individual components of Bwd conversion: */

static int
icxLuMonoBwd_abs (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icmPe_lurv prv;
	int rv = 0;

	if (p->pcs == icxSigJabData) {
		p->cam->cam_to_XYZ(p->cam, out, in);
		/* Hack to prevent CAM02 weirdness being amplified by */
		/* any later per channel clipping. */
		/* Limit -Y to non-stupid values by scaling */
		if (out[1] < -0.1) {
			out[0] *= -0.1/out[1];
			out[2] *= -0.1/out[1];
			out[1] = -0.1;
		}
		if (p->dir) {
			prv = p->plu->input_fmt_fwd(p->plu, out, out);
			prv |= p->plu->input_pch_fwd(p->plu, out, out);	/* Should be NOP */
		} else {
			prv = p->plu->output_fmt_bwd(p->plu, out, out);
			prv |= p->plu->output_pch_bwd(p->plu, out, out);	/* Should be NOP */
		}
		rv = LUE2XLUE(prv);
	} else {
		if (p->dir) {
			prv = p->plu->input_fmt_fwd(p->plu, out, in);
			prv |= p->plu->input_pch_fwd(p->plu, out, out);	/* Should be NOP */
		} else {
			prv = p->plu->output_fmt_bwd(p->plu, out, in);
			prv |= p->plu->output_pch_bwd(p->plu, out, out);	/* Should be NOP */
		}
		rv = LUE2XLUE(prv);
	}
	return rv;
}

static int
icxLuMonoBwd_map (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icmPe_lurv prv;
	if (p->dir) {
		prv = p->plu->core5_fwd(p->plu, out, in);
	} else {
		prv = p->plu->core5_bwd(p->plu, out, in);
	}
	return LUE2XLUE(prv);
}

static int
icxLuMonoBwd_curve (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icmPe_lurv prv;
	if (p->dir) {
		prv = p->plu->output_pch_fwd(p->plu, out, in);
		prv |= p->plu->output_fmt_fwd(p->plu, out, out);	/* Should be NOP */
	} else {
		prv = p->plu->input_pch_bwd(p->plu, out, in);
		prv |= p->plu->input_fmt_bwd(p->plu, out, out);	/* Should be NOP */
	}
	return LUE2XLUE(prv);
}

/* Overall Bwd conversion routine */
/* Note that the overall conversion is in the requested direction, */
/* as the setup code swaps icxLuMonoxFwd_lookup/icxLuMonoxBwd_lookup */
/* as needed. */
static int
icxLuMonoBwd_lookup (
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	double temp[3];
	int rv = 0;
	icxLuMono *p = (icxLuMono *)pp;
	rv |= icxLuMonoBwd_abs(p, temp, in);
	rv |= icxLuMonoBwd_map(p, out, temp);
	rv |= icxLuMonoBwd_curve(p, out, out);
	return rv;
}

static void
icxLuMono_free(
icxLuBase *p
) {
	p->plu->del(p->plu);
	if (p->cam != NULL)
		p->cam->del(p->cam);
	free(p);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given a nominated output PCS (ie. Absolute, Jab etc.), convert it in the bwd */
/* direction into a relative XYZ or Lab PCS value */
/* (This is used in generating gamut compression in B2A tables) */
void icxLuMono_bwd_outpcs_relpcs(
icxLuBase *pp,
icColorSpaceSignature os,		/* Output space, XYZ or Lab */
double *out, double *in) {
	icxLuMono *p = (icxLuMono *)pp;

	if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icxLuMonoFwd_abs(p, out, in);
		icmLab2XYZ(&icmD50, out, out);
	} else if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icxLuMonoFwd_abs(p, out, in);
		icmXYZ2Lab(&icmD50, out, out);
	} else {
		icxLuMonoFwd_abs(p, out, in);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static gamut *icxLuMonoGamut(icxLuBase *plu, double detail);
static icxCuspMap *icxLuMonoCuspMap(icxLuBase *plu, int res) { return NULL; };

static icxLuBase *
new_icxLuMono(
xicc                  *xicp,
int                   flags,		/* clip, merge flags */
icmLuSpace            *plu,			/* Pointer to Lu we are expanding */
icmLookupFunc         func,			/* Functionality requested */
icRenderingIntent     intent,		/* Rendering intent */
icColorSpaceSignature pcsor,		/* PCS override (0 = def) */
icxViewCond           *vc,			/* Viewing Condition (NULL if pcsor is not CIECAM) */
int                   dir			/* 0 = fwd, 1 = bwd */
) {
	icxLuMono *p;

	/* Do the basic icxLuMono creation and initialisation */

	if ((p = (icxLuMono *) calloc(1,sizeof(icxLuMono))) == NULL)
		return NULL;

	p->lutype            = icxLuMonoType;
	p->pp                = xicp;
	p->plu               = plu;
	p->del               = icxLuMono_free;
	p->lutspaces         = icxLutSpaces;
	p->spaces            = icxLuSpaces;
	p->get_native_ranges = icxLu_get_native_ranges;
	p->get_ranges        = icxLu_get_ranges;
	p->efv_wh_bk_points  = icxLuEfv_wh_bk_points;
	p->get_gamut         = icxLuMonoGamut;
	p->get_cuspmap       = icxLuMonoCuspMap;
	p->fwd_relpcs_outpcs = icxLuMono_fwd_relpcs_outpcs;
	p->bwd_outpcs_relpcs = icxLuMono_bwd_outpcs_relpcs;
	p->nearclip = 0;				/* Set flag defaults */
	p->mergeclut = 0;
	p->intsep = 0;

	p->dir = dir;

	p->fwd_lookup = icxLuMonoFwd_lookup;
	p->fwd_curve  = icxLuMonoFwd_curve;
	p->fwd_map    = icxLuMonoFwd_map;
	p->fwd_abs    = icxLuMonoFwd_abs;
	p->bwd_lookup = icxLuMonoBwd_lookup;
	p->bwd_abs    = icxLuMonoBwd_abs;
	p->bwd_map    = icxLuMonoBwd_map;
	p->bwd_curve  = icxLuMonoBwd_curve;
	if (dir) {
		p->lookup     = icxLuMonoBwd_lookup;
		p->inv_lookup = icxLuMonoFwd_lookup;
	} else {
		p->lookup     = icxLuMonoFwd_lookup;
		p->inv_lookup = icxLuMonoBwd_lookup;
	}

	/* There are no mono specific flags */
	p->flags = flags;
	p->func = func;

	/* Remember the effective intent */
	p->intent = intent;

	/* In general the native and effective space info of the icx will be the same as the */
	/* underlying icm lookup object. */
	{
		icmCSInfo ini, outi, pcsi;

		/* Get details of internal, native color space */
		plu->native_spaces(p->plu, &ini, &outi, &pcsi);
		p->natis = ini.sig;
		p->natos = outi.sig;
		p->natpcs = pcsi.sig;

		icmCpyN(p->ninmin, ini.min, ini.nch);
		icmCpyN(p->ninmax, ini.max, ini.nch);
		icmCpyN(p->noutmin, outi.min, outi.nch);
		icmCpyN(p->noutmax, outi.max, outi.nch);

		/* Get details of external, effective color spaces */
		plu->spaces(p->plu, &ini, &outi, &pcsi, NULL, NULL, NULL, NULL, NULL, NULL);
		p->ins = ini.sig;
		p->inputChan = ini.nch;
		p->outs = outi.sig;
		p->outputChan = outi.nch;
		p->pcs = pcsi.sig;

		icmCpyN(p->inmin, ini.min, ini.nch);
		icmCpyN(p->inmax, ini.max, ini.nch);
		icmCpyN(p->outmin, outi.min, outi.nch);
		icmCpyN(p->outmax, outi.max, outi.nch);
	}

	/* Init the CAM model */
	if (pcsor == icxSigJabData) {
		p->vc  = *vc;				/* Copy the structure */
		p->cam = new_icxcam(cam_default);
		p->cam->set_view_vc(p->cam, vc);
	} else 
		p->cam = NULL;

	/* Override with pcsor */
	if (pcsor == icxSigJabData) {
		p->pcs = pcsor;		
		if (func == icmBwd || func == icmGamut || func == icmPreview)
			p->ins = pcsor;
		if (func == icmFwd || func == icmPreview)
			p->outs = pcsor;
	}

	/* If we have a Jab PCS override, reflect this in the effective icx range. */
	/* Note that the ab ranges are nominal. They will exceed this range */
	/* for colors representable in L*a*b* PCS */
	if (p->ins == icxSigJabData) {
		p->inmin[0] = 0.0;		p->inmax[0] = 100.0;
		p->inmin[1] = -128.0;	p->inmax[1] = 128.0;
		p->inmin[2] = -128.0;	p->inmax[2] = 128.0;
	} else if (p->outs == icxSigJabData) {
		p->outmin[0] = 0.0;		p->outmax[0] = 100.0;
		p->outmin[1] = -128.0;	p->outmax[1] = 128.0;
		p->outmin[2] = -128.0;	p->outmax[2] = 128.0;
	} 

	return (icxLuBase *)p;
}

/* ============================================================= */

/* Given an xicc lookup object, return a gamut object. */
/* Note that the PCS must be Lab or Jab */
/* Return NULL on error, check e.c+e.m for reason */
static gamut *icxLuMonoGamut(
icxLuBase   *plu,		/* this */
double       detail		/* gamut detail level, 0.0 = def */
) {
	gamut *xgam;
	xicc  *p = plu->pp;				/* parent xicc */

	p->e.c = 1;
	sprintf(p->e.m,"Creating Mono gamut surface not supported yet.");
	plu->del(plu);
	xgam = NULL;

	return xgam;
}

