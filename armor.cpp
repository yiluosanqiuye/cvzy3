#include "armor.h"

//-----------------------------------------主流程函数-----------------------------------------------------

Mat Photo::flipped(const Mat &image) {
    Mat image_flipped;

    flip(image,image_flipped,-1);
    return image_flipped;
}


Mat Photo::get_mask(const Mat &image) {
    Mat gray, blurred, mask;

    cvtColor(image, gray, COLOR_BGR2GRAY);
    GaussianBlur(gray, blurred, Size(3, 3), 0);
    threshold(blurred, mask, 200, 255, THRESH_BINARY);

    return mask;
}


Mat Photo::mask_morphology(const Mat &mask) {
    Mat kernel = getStructuringElement(MORPH_RECT, Size(1, 3));

    Mat mask_close;

    morphologyEx(mask, mask_close, MORPH_CLOSE, kernel);

    return mask_close;
}


Light::Light(const RotatedRect &r,const Mat &image): rect(r), center(r.center) {
    long_side  = max(r.size.width, r.size.height);
    short_side = min(r.size.width, r.size.height);
    angle      = get_angle(r);
    get_color(image);
}


vector<Light> Photo::get_lights(const Mat &mask,const Mat &image) {
    //准备存放轮廓的数组和mask
    vector<vector<Point>> contours;
    Mat mask_clone=mask.clone();

    //收集轮廓
    findContours(mask_clone, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    //收集所有灯带
    vector<Light> lights;
    for ( const auto& contour : contours) {
        //1.面积筛选
        double area=contourArea(contour);
        cout << "候选面积：" << area << endl;
        if (area<30||area>2000) {
            continue;
        }

        //2.长宽比筛选
        RotatedRect c_rect=minAreaRect(contour);
        float c_long_side=max(c_rect.size.width,c_rect.size.height);
        float c_short_side=min(c_rect.size.width,c_rect.size.height);
        float ratio=c_long_side/c_short_side;

        if (ratio<3.0) {
            continue;
        }

        //收集灯带信息
        Light l(c_rect,image);
        lights.push_back(l);
    }

    cout << "灯条候选数: " << lights.size() << endl;
    return lights;
}


void Photo::draw_lights( Mat &canva, const vector<Light> &lights) {

    for (int i = 0; i < (int)lights.size(); i++) {
        const Light &l=lights[i];

        Point2f corners[4];
        l.rect.points(corners);
        vector<Point> light_corners(corners, corners + 4);

        polylines(canva, light_corners, true, Scalar(0,0,255), 2);
        circle(canva, l.center, 3, Scalar(0,255,0), -1);
        putText(canva, to_string(i),
                l.center + Point2f(-14, -l.long_side * 0.5f - 6),
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 1);
    }

}


bool can_pair(const Light &l1, const Light &l2) {
    //a.颜色一致（允许有一方不确定）
    if (l1.color != l2.color &&
        l1.color != "unknown" && l2.color != "unknown") {
        return false;
        }

    //b.长度大致相近
    float ratio = l1.long_side / l2.long_side;
    if (ratio > 1.6f || ratio < 0.6f) {
        return false;
    }

    float aver_long = (l1.long_side + l2.long_side) * 0.5f;

    //c.方向协调：两根长轴的夹角不能太大
    if (abs(l1.angle - l2.angle) > 20.f) {
        return false;
    }

    //d.中心的上/下偏差
    float dy = abs(l1.center.y - l2.center.y);
    if (dy > 0.5f * aver_long) {
        return false;
    }

    //e.中心的左/右偏差
    float dx = abs(l1.center.x - l2.center.x);
    if (dx < 1.5f * aver_long || dx > 6.0f * aver_long) {
        return false;
    }

    return true;
}



vector<pair<int, int>> get_pairlights(const vector<Light> &lights) {
    vector<pair<int, int>> pairlights;
    //记录灯带的配对情况，先初始化全部未配对
    vector<bool> used(lights.size(), false);

    for (int i = 0; i < (int)lights.size(); i++) {
        if (used[i]) continue;
        const Light &l1 = lights[i];

        for (int j = i + 1; j < (int)lights.size(); j++) {
            if (used[j]) continue;
            const Light &l2 = lights[j];

            if (!can_pair(l1, l2)) continue;

            //保证左侧灯条编号在前
            if (lights[i].center.x <= lights[j].center.x) {
                pairlights.emplace_back(i, j);
            } else {
                pairlights.emplace_back(j, i);
            }

            used[i] = used[j] = true;
            break;
        }
    }

    return pairlights;
}


Armor::Armor( const Light &l1, const Light &l2,int left_id,int right_id,const Mat &image) {
    // 1.获得构成灯带的编号
    left_light_id  = left_id;
    right_light_id = right_id;

    // 2.由8个灯带顶点确定最小外接矩形
    vector<Point2f> pts(8);
    l1.rect.points(&pts[0]);
    l2.rect.points(&pts[4]);

    RotatedRect pairlights_rect = minAreaRect(pts);

    // 3.获得装甲板的外接框、中心点、四角点
    rect    = pairlights_rect;
    center  = pairlights_rect.center;
    corners = get_rect_corners(pairlights_rect);

    // 4.获得装甲板颜色（允许一条灯带颜色未知）
    if      (l1.color==l2.color)    color=l1.color;
    else if (l1.color == "unknown") color = l2.color;
    else if (l2.color == "unknown") color = l1.color;
    else                            color = "unknown";

    // 5.获得有效性
    check_valid(image);
}


void delete_repeat_armors(vector<Armor> &armors) {
    vector<Armor> keep;

    for (const Armor &a:armors) {
        bool repeat=false;

        for (const Armor &k:keep) {
            float threshold = 0.3f * max(a.rect.size.width, a.rect.size.height);

            //x、y 都比阈值近，就算同一块
            if (abs(a.center.x - k.center.x) < threshold &&
                abs(a.center.y - k.center.y) < threshold) {
                repeat = true;
                break;
                }
        }

        if (!repeat) {
            keep.push_back(a);
        }
    }

    armors = keep;
}


void Photo::draw_armor(Mat &canva,const Armor &armor) {

    // 1.画外接框
    vector<Point> corners_int(armor.corners.begin(), armor.corners.end());
    polylines(canva, corners_int, true, Scalar(0,0,255), 2);

    // 2.显示角点编号
    for (int i = 0; i < 4; i++) {
        circle(canva, armor.corners[i], 3, Scalar(0,0,255), -1);
        Point2f offset[] = { {-20, -8}, {6, -8}, {6, 12}, {-20, 12} };
        putText(canva,
            to_string(i),
                armor.corners[i] + offset[i],   // 防止编号盖住框
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0,255,0), 1);
    }

    // 3.画中心点
    circle(canva, armor.center, 3, Scalar(0,255,0), -1);
}


void Armor::print_information() const{
    cout << "===== 装甲板 =====" << endl;
    cout << "颜色：" << color << endl;
    cout << "左灯条：编号" << left_light_id << "  "
         << "右灯条：编号" << right_light_id << endl;
    cout << "外接框：" << rect.size.width << " x " << rect.size.height
         << "  角度：" << rect.angle << "°" << endl;
    cout << "中心：("  << center.x << ", " << center.y << ")" << endl;
    cout << "角点：";
    for (int i = 0; i < 4; i++) {
        cout << i << "(" << corners[i].x << ","
             << corners[i].y << ") ";
    }
    cout << endl;
    cout << "目标" << (valid ? "有效" : "无效") << endl;
}



//---------------------------------------其他函数-------------------------------------------------------

void Light::get_color(const Mat &image) {

    color="unknown";

    //roi裁剪只能对正矩形使用，把灯的旋转矩形先转换成一个正矩形box
    Rect box=rect.boundingRect();
    box.x=max(box.x, 0);
    box.y=max(box.y, 0);
    box.width=min(box.x + box.width,  image.cols) - box.x;
    box.height=min(box.y + box.height, image.rows) - box.y;

    //在原彩图中把灯的区域提取出来
    Mat roi=image(box);
    Mat roi_hsv;
    cvtColor(roi,roi_hsv,COLOR_BGR2HSV);

    //筛选出有效的像素块
    vector<int> h;
    for (int y = 0; y < roi_hsv.rows; y++) {
        // 获取hsv图像第y行的只读像素起始指针p（存该行首地址），Vec3b为3通道uchar像素
        const Vec3b *p = roi_hsv.ptr<Vec3b>(y);
        // 遍历当前行所有列像素
        for (int x = 0; x < roi_hsv.cols; x++) {
            // p[x]：取出当前行第x个像素（Vec3b类型）    p[x][0]：分别得到该像素的H、S、V分量
            uchar H = p[x][0];
            uchar S = p[x][1];
            uchar V = p[x][2];
            // 筛选饱和度、亮度达标，保留有效彩色像素
            if (S > 60 && V > 100) {
                h.push_back(H);
            }
        }
    }

    //若搜集到像素过少则退出
    if (h.size()<20) {
        return;
    }

    //取所有像素颜色的中位数
    sort(h.begin(),h.end());
    int median_h=h[h.size()/2];

    //根据h值判定颜色
    if (median_h<30||median_h>160) {
        color = "red";
    }
    else if (median_h>=100&&median_h<=130) {
        color = "blue";
    }
}


array<Point2f, 4> get_rect_corners(const RotatedRect &r) {
    // 1.先得到两个边的单位向量作为方向基准
    float th = r.angle * PI / 180.f;
    Point2f width_dir(cos(th), sin(th));
    Point2f height_dir(cos(th+PI/2), sin(th+PI/2));

    // 2.找更贴近原图水平的一边作为x轴基准
    bool use_w = abs(width_dir.x) >= abs(height_dir.x);

    // 3.让a朝右，b朝下(相当于新的x,y轴正方向）
    Point2f a = use_w ? width_dir : height_dir;
    if (a.x < 0) a = -a;
    Point2f b(-a.y, a.x);

    // 4.从中心出发，各走两个半步前往四个角
    array<Point2f, 4> corners;
    float pa = (use_w ? r.size.width  : r.size.height) * 0.5f;
    float pb = (use_w ? r.size.height : r.size.width ) * 0.5f;
    corners[0] = r.center - a * pa - b * pb;   // 左上
    corners[1] = r.center + a * pa - b * pb;   // 右上
    corners[2] = r.center + a * pa + b * pb;   // 右下
    corners[3] = r.center - a * pa + b * pb;   // 左下
    return corners;
}



void Armor::check_valid(const Mat &image) {
    valid = false;

    //1.角点位置检查
    for (int i = 0; i < 4; i++) {
        if (corners[i].x < 0 || corners[i].x >= image.cols ||
            corners[i].y < 0 || corners[i].y >= image.rows) {
            return;
            }
    }

    //2.面积检查
    if (rect.size.width * rect.size.height < 1.0f) {
        return;
    }

    //3.长宽比检查
    float armor_width=max(rect.size.width, rect.size.height);
    float armor_height=min(rect.size.width, rect.size.height);
    float armor_ratio=armor_width/armor_height;
    if (armor_ratio < 1.5f || armor_ratio > 6.0f) {
        return;
    }

    //全过置为有效
    valid = true;
}


float get_angle(const RotatedRect &r) {
    //把旋转矩形的角度转成弧度，并定义width和height两个方向上的单位向量
    float th = r.angle * PI / 180.f;
    Point2f width_dir(cos(th), sin(th));
    Point2f heigth_dir(cos(th+PI/2), sin(th+PI/2));

    //长边为长轴
    Point2f longdir=(r.size.width >= r.size.height)?width_dir:heigth_dir;

    //统一让长轴方向指向“下”
    if (longdir.y < 0) longdir = -longdir;

    //函数atan2(x, y)，得到从「+y 轴」量起，到向量(x, y)的带符号角度
    return atan2(longdir.x, longdir.y) * 180.f / PI;
}





//---------------------------------------单帧检测接口---------------------------------------------------
vector<Armor> detect_armors(const Mat &image, DetectDebug *debug) {
    Photo demo;

    //1.mask
    Mat mask            = demo.get_mask(image);
    Mat mask_morphology = demo.mask_morphology(mask);

    //2.灯条
    vector<Light> lights = demo.get_lights(mask_morphology, image);

    //3.调试信息交给调用方
    if (debug!=nullptr) {
        debug->mask            = mask;
        debug->mask_morphology = mask_morphology;
        debug->lights          = lights;
    }

    //4.灯条配对
    vector<pair<int,int>> pairlights = get_pairlights(lights);

    //5.装甲板
    vector<Armor> armors;

    for (const auto &p : pairlights) {
        Armor armor(lights[p.first], lights[p.second],p.first,p.second, image);
        armors.push_back(armor);
    }

    delete_repeat_armors(armors);



    return armors;
}