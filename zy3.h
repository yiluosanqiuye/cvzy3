#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>
#include <algorithm>


using namespace cv;
using namespace std;


class Light {
public:
    string color="unknown";
    float long_side;
    float short_side;
    float angle;        //长轴与竖直方向夹角
    RotatedRect rect;
    Point2f center;

    Light(const RotatedRect &r);

    void get_color(const Mat &image);

};

float get_angle(const RotatedRect &r);

vector<pair<int,int>> get_pairlights(const vector<Light> &lights);

array<Point2f, 4> get_rect_corners(const RotatedRect &r);


class Armor {
public:
    string color="unknown";
    int left_light_id=-1;
    int right_light_id=-1;
    RotatedRect rect;
    Point2f center;
    array<Point2f, 4> corners;   // 0左上 1右上 2右下 3左下
    bool valid=false;                  //是否有效

    Armor(const Light &l1, const Light &l2, const Mat &image);

    void check_valid(const Mat &image);

};


void delete_repeat_armors(vector<Armor> &armors);

class Photo {
public:
    Mat flipped(const Mat &image);
    Mat get_mask(const Mat &image);
    Mat mask_morphology(const Mat &mask);
    vector<Light> get_lights(const Mat &mask,Mat &image);
    void draw_lights(Mat &canva, const vector<Light> &lights);
    void draw_armor(Mat &canva,const Armor &armor);
};
