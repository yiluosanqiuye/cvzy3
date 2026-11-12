#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>
#include <algorithm>

using namespace cv;
using namespace std;

const float PI=CV_PI;


//类

class Light {
public:
    string color="unknown";
    float long_side;
    float short_side;
    float angle;        //长轴与竖直方向夹角
    RotatedRect rect;
    Point2f center;

    Light(const RotatedRect &r,const Mat &image);

private:
    void get_color(const Mat &image);
};

vector<pair<int,int>> get_pairlights(const vector<Light> &lights);





class Armor {
public:
    string color="unknown";
    int left_light_id=-1;
    int right_light_id=-1;
    RotatedRect rect;
    Point2f center;
    array<Point2f, 4> corners;   // 0左上 1右上 2右下 3左下
    bool valid=false;                  //是否有效

    Armor(const Light &l1, const Light &l2,int left_id,int right_id,const Mat &image);

    void check_valid(const Mat &image);
    void print_information()const;
};

void delete_repeat_armors(vector<Armor> &armors);





class Photo {
public:
    Mat flipped(const Mat &image);
    Mat get_mask(const Mat &image);
    Mat mask_morphology(const Mat &mask);
    vector<Light> get_lights(const Mat &mask,const Mat &image);
    void draw_lights(Mat &canva, const vector<Light> &lights);
    void draw_armor(Mat &canva,const Armor &armor);
};



//功能函数

array<Point2f, 4> get_rect_corners(const RotatedRect &r);
float get_angle(const RotatedRect &r);





// 检测过程的调试信息（可选输出）
struct DetectDebug {
    Mat           mask;              // 原始mask
    Mat           mask_morphology;   // 形态学后的mask
    vector<Light> lights;            // 灯条
};

// 单帧检测：输入一张彩色图，返回装甲板候选列表        debug 为可选输出，传 nullptr 表示不需要调试图（视频逐帧时用）
vector<Armor> detect_armors(const Mat &image, DetectDebug *debug = nullptr);