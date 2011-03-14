/*
********************************************************************
(c) MSU Video Group 2003-2010, http://compression.ru/video/
This source code is property of MSU Graphics and Media Lab
This code can be distributed and used only with WRITTEN PERMISSION.
********************************************************************
	Author: Napadovsky Vyacheslav
	Group: 217
********************************************************************
*/

#include "types.h"
#include "math.h"
using namespace std;

#define ENABLE_SPLIT

struct metric {
	long value;
//	long derivative;
	Point p;
};

class MyData {
	BYTE *cur_Y_frame, *prev_Y_frame, *prev_Y_frame_up, *prev_Y_frame_l, *prev_Y_frame_upl;
	MV *MVectors;
	int width, height;
	BYTE quality;
	bool use_half_pixel;

	int num_blocks_vert, num_blocks_hor, wext, max_offset; 
	MV prob_motion_vector, *curvector;
	BYTE *cur, *prev;
	int vert_offset, hor_offset;
	int first_row_offset, mm;

	long (*metricproc)(const BYTE*, const BYTE*, const int);
	void (MyData::*addmetric)(int x, int y);
	metric metrics[100];
	int mcount;
	MV *prevmotion;

	int sh_startpattern;		// 3, 2, 1
	bool sh_check_nbh;			// true, false
	int sh_steps;				// inf, 5, 3, 2, 1
	int sh_use_split;

public:
	MyData(int width, int height, BYTE quality);
	~MyData();
	void DoAction(BYTE* cur_Y_frame, BYTE* prev_Y_frame, BYTE* prev_Y_frame_up, 
		BYTE* prev_Y_frame_l, BYTE* prev_Y_frame_upl, MV *MVectors,
		bool use_half_pixel);
	void DoBlocks();
	void DoHalfPixel(int x, int y);
	void DoSplit(int x, int y);
	void DoBlock(int x, int y);
	void addmetric_def(int x, int y);
	void addmetric_half(int x, int y);
	void addaround(int x, int y, int pattern);
	int getbestmetric();
};

MyData::MyData(int width, int height, BYTE quality) : width(width),height(height),quality(quality) {
	mm = MAX_MOTION;
	num_blocks_vert = (height + 15) >> 4;
	num_blocks_hor = (width + 15) >> 4;
	wext = width + BORDER * 2;
	first_row_offset = wext * BORDER + BORDER;
	max_offset = (width + BORDER*2) * (height + BORDER*2) - 16*wext;
	prevmotion = new MV[num_blocks_vert*num_blocks_hor];
	metricproc = GetErrorSAD_16x16;
}

MyData::~MyData()
{
	delete[] prevmotion;
}

void MyData::addmetric_def(int x, int y) {
	BYTE *p = prev + wext*y + x;
	if (p - prev_Y_frame >= max_offset)
		return;

	metric &m = metrics[mcount];
	m.value = metricproc(cur, p, wext);
	/*if (!x && (x == y)) {
		m.derivative = 0;
	} else
		m.derivative = (m.value - metrics[0].value)/(x*x+y*y);*/
	m.p.x = x;
	m.p.y = y;
	mcount++;
}

void MyData::addmetric_half(int x, int y) {
	int dx = x%2;
	int dy = y%2;
	BYTE *p;
	metric& m = metrics[mcount];
	m.p.x = x;
	m.p.y = y;
	x /= 2;
	y /= 2;
	int add = vert_offset + hor_offset + wext*y + x;
	if (add >= max_offset)
		return;
	if (dx && dy)
		p = prev_Y_frame;
	else if (dx)
		p = prev_Y_frame_up;
	else if (dy)
		p = prev_Y_frame_l;
	else
		p = prev_Y_frame_upl;
	p += add;
	m.value = metricproc(cur, p, wext);
	mcount++;
}

__forceinline int MyData::getbestmetric() {
	long best = metrics[0].value;
	int best_num = 0;
	for (int i=1; i<mcount; i++)
		if (best > metrics[i].value) {
			best = metrics[i].value;
			best_num = i;
		}
	return best_num;
}

const Point pattern1[] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };
const Point pattern2[] = { {0, 2}, {2, 0}, {0, -2}, {-2, 0}, {1, 1}, {1, -1}, {-1, -1}, {-1, 1} };
const Point pattern3[] = { {0, 5}, {5, 0}, {0, -5}, {-5, 0}, {2, 2}, {2, -2}, {-2, -2}, {-2, 2} };

const int patterncount = 3;
const int pat_point_count[patterncount] = {4, 8, 8};
const Point *patterns[patterncount] = {pattern1, pattern2, pattern3};

__forceinline void MyData::addaround(int x, int y, int pattern) {
	const Point *p = patterns[pattern];
	int t = pat_point_count[pattern];
	for (int i=0; i<t; i++) {
		(this->*addmetric)(x + p->x, y + p->y);
		p++;
	}
}

__forceinline void MyData::DoBlock(int x, int y) {
	const Point *pattern;
	int best_num, pattnum, steps = sh_steps;
	Point& p = metrics[0].p;
	MV *m;

	mcount = 0;
	(this->*addmetric)(0, 0);
	if (sh_check_nbh) addaround(0, 0, sh_startpattern);
	if (x != 0) {
		m = &prevmotion[num_blocks_hor*y + x-1];
		(this->*addmetric)(m->x, m->y);
		if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
	}
	if (x != num_blocks_hor-1) {
		m = &prevmotion[num_blocks_hor*y + x+1];
		(this->*addmetric)(m->x, m->y);
		if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
	}
	if (y != 0) {
		m = &prevmotion[num_blocks_hor*(y-1) + x];
		(this->*addmetric)(m->x, m->y);
		if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
	}
	if (y != num_blocks_vert-1) {
		m = &prevmotion[num_blocks_hor*(y+1) + x];
		(this->*addmetric)(m->x, m->y);
		if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
	}

	if (quality > 60) {
		if ((x != 0) && (y != 0)) {
			m = &prevmotion[num_blocks_hor*(y-1) + x-1];
			(this->*addmetric)(m->x, m->y);
			if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
		}
		if ((x != 0) && (y != num_blocks_vert-1)) {
			m = &prevmotion[num_blocks_hor*(y+1) + x-1];
			(this->*addmetric)(m->x, m->y);
			if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
		}
		if ((x != num_blocks_hor-1) && (y != 0)) {
			m = &prevmotion[num_blocks_hor*(y-1) + x+1];
			(this->*addmetric)(m->x, m->y);
			if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
		}
		if ((x != num_blocks_hor-1) && (y != num_blocks_vert-1)) {
			m = &prevmotion[num_blocks_hor*(y+1) + x+1];
			(this->*addmetric)(m->x, m->y);
			if (sh_check_nbh) addaround(m->x, m->y, sh_startpattern);
		}
	}

	best_num = getbestmetric();
	metrics[0] = metrics[best_num];
	pattnum = sh_startpattern;

	if (use_half_pixel) {
		x = x*2 + 1;
		y = y*2 + 1;
		steps *= 2;
	}

	while (steps--) {
		addaround(p.x, p.y, pattnum);
		best_num = getbestmetric();
		metrics[0] = metrics[best_num];
		if (best_num == 0) {
			if (!--pattnum)
				break;
		} else
			pattnum = sh_startpattern;
		mcount = 1;
	}

	if (use_half_pixel) {
		int dx = p.x % 2;
		int dy = p.y % 2;
		if (dx && dy)
			prob_motion_vector.dir = sd_none;
		else if (dx)
			prob_motion_vector.dir = sd_up;
		else if (dy)
			prob_motion_vector.dir = sd_l;
		else
			prob_motion_vector.dir = sd_upl;
		p.x /= 2;
		p.y	/= 2;
	}

	prob_motion_vector.x = p.x;
	prob_motion_vector.y = p.y;
	prob_motion_vector.error = metrics[0].value;
}

//////////////////////////////////////////////////////////////////////////////////////////

__forceinline void MyData::DoAction(BYTE* cur_Y_frame, BYTE* prev_Y_frame, BYTE* prev_Y_frame_up, 
	BYTE* prev_Y_frame_l, BYTE* prev_Y_frame_upl, MV *MVectors,
	bool use_half_pixel)
{
	this->cur_Y_frame = cur_Y_frame;
	this->prev_Y_frame = prev_Y_frame;
	this->prev_Y_frame_up = prev_Y_frame_up;
	this->prev_Y_frame_l = prev_Y_frame_l;
	this->prev_Y_frame_upl = prev_Y_frame_upl;
	this->MVectors = MVectors;
	this->use_half_pixel = use_half_pixel;


	addmetric = use_half_pixel ? (&MyData::addmetric_half) : (&MyData::addmetric_def);
	// Quality 20, 40, 60, 80, 100
	sh_use_split = true;
	sh_check_nbh = true;
	if (quality > 80) {
		sh_startpattern = use_half_pixel ? 3 : 2;
		sh_steps = MAXINT;
	} else
	if (quality > 60) {
		sh_startpattern = 2;
		sh_steps = 6;
	} else
	if (quality > 40) {
		sh_startpattern = 2;
		sh_steps = 4;
	} else
	if (quality > 20) {
		sh_startpattern = 2;
		sh_use_split = false;
		sh_check_nbh = false;
		sh_steps = 3;
	} else {
		sh_startpattern = 1;
		sh_use_split = false;
		sh_check_nbh = false;
		sh_steps = 2;
	}
	if (use_half_pixel)
		sh_steps *= 2;

	DoBlocks();
}

void MyData::DoBlocks() {
	int k, l, temp;
	int x = 0,
		y = 0,
		pos_left = 0,
		pos_top = 1,
		pos_right = num_blocks_hor-1,
		pos_bottom = num_blocks_vert-1,
		dist = 0; // right (0), bottom (1), left (2), top (3)
	for (int pos=0; pos < num_blocks_hor*num_blocks_vert; pos++)
	{
		vert_offset = (y << 4) * wext + first_row_offset;
		hor_offset = (x << 4);
		cur = cur_Y_frame + vert_offset + hor_offset;
		prev = prev_Y_frame + vert_offset + hor_offset;

		prob_motion_vector.dir = sd_none;
		curvector = &MVectors[y * num_blocks_hor + x];
		DoBlock(x, y);
		*curvector = prob_motion_vector;

		MV& pm = prevmotion[y * num_blocks_hor + x];
		pm = prob_motion_vector;
		if (use_half_pixel) {
			pm.x = pm.x * 2 + ((pm.dir == sd_none) || (pm.dir == sd_up));
			pm.y = pm.y * 2 + ((pm.dir == sd_none) || (pm.dir == sd_l));
		}
#ifdef ENABLE_SPLIT
		if (sh_use_split && (prob_motion_vector.error > SPLIT_TRESH))
			DoSplit(x, y);
#endif
		switch (dist) {
		case 0: // right
		label0:
			if (x != pos_right) {
				x++;
				break;
			} else {
				pos_right--;
				dist++;
			}
		case 1: // bottom
			if (y != pos_bottom) {
				y++;
				break;
			} else {
				pos_bottom--;
				dist++;
			}
		case 2: // left
			if (x != pos_left) {
				x--;
				break;
			} else {
				pos_left++;
				dist++;
			}
		case 3: // top
			if (y != pos_top) {
				y--;
				break;
			} else {
				pos_top++;
				dist = 0;
				goto label0;
			}
		default:
			break;
		}
	}
}

__forceinline void MyData::DoSplit(int x, int y) {
	metricproc = GetErrorSAD_8x8;
	curvector->splitted = true;
	for(int h = 0; h < 4; h++) {
		prob_motion_vector = MV();
		prob_motion_vector.dir = sd_none;

		vert_offset = ((y << 4) + ((h > 1)? 8 : 0))* wext + first_row_offset;
		hor_offset = (x << 4) + ((h & 1)? 8 : 0);
		cur = cur_Y_frame + vert_offset + hor_offset;
		prev = prev_Y_frame + vert_offset + hor_offset;

		DoBlock(x,y);
		if (curvector->sub[h] == NULL)
			curvector->sub[h] = new MV(prob_motion_vector);
	}
	if (curvector->sub[0]->error + curvector->sub[1]->error + 
		curvector->sub[2]->error + curvector->sub[3]->error > curvector->error*0.7)
	{
		curvector->splitted = false;
	}
	metricproc = GetErrorSAD_16x16;
}


////////////////////////////////////////////////////////////////////////////////////////
	
MyData *MyObj = NULL;

void MEFunction(BYTE* cur_Y_frame, BYTE* prev_Y_frame, BYTE* prev_Y_frame_up, 
	BYTE* prev_Y_frame_l, BYTE* prev_Y_frame_upl, int width, int height, MV *MVectors,
	BYTE quality, bool use_half_pixel)
{
	MyObj->DoAction(cur_Y_frame, prev_Y_frame, prev_Y_frame_up, prev_Y_frame_l,
		prev_Y_frame_upl, MVectors, use_half_pixel);
}

void MEStart(int width, int height, BYTE quality)
{
	if (MyObj != NULL)
		delete MyObj;
	MyObj = new MyData(width, height, quality);
}

void MEEnd()
{
	delete MyObj;
	MyObj = NULL;
}
