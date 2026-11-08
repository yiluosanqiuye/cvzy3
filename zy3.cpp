#include "zy3.h"

//-----------------------------------------主流程函数-----------------------------------------------------

Mat Photo::flipped(const Mat &image) {
    Mat image_flipped;

    //翻转图像
    flip(image,image_flipped,0);
    return image_flipped;
}


Mat Photo::get_mask(const Mat &image) {
    Mat gray, blurred, mask;

    cvtColor(image, gray, COLOR_BGR2GRAY);
    GaussianBlur(gray, blurred, Size(3, 3), 0);
    threshold(blurred, mask, 200, 255, THRESH_BINARY);

    imwrite("output/target_mask.png", mask);
    return mask;
}


Mat Photo::mask_morphology(const Mat &mask) {
    Mat kernel = getStructuringElement(MORPH_RECT, Size(1, 3));

    Mat mask_close;

    morphologyEx(mask, mask_close, MORPH_CLOSE, kernel);

    imwrite("output/mask_close.png", mask_close);
    return mask_close;
}


Light::Light(const RotatedRect &r)
    : rect(r), center(r.center) {
    long_side  = max(r.size.width, r.size.height);
    short_side = min(r.size.width, r.size.height);
    angle      = get_angle(r);
}


vector<Light> Photo::get_lights(const Mat &mask,Mat &image) {
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
        Light l(c_rect);
        l.get_color(image);
        lights.push_back(l);
    }

    cout << "灯条候选数: " << lights.size() << endl;
    return lights;
}


// 在图像副本上画出所有灯条候选：旋转矩形 + 中心点 + 编号
void Photo::draw_lights( Mat &canva, const vector<Light> &lights) {

    for (int i = 0; i < (int)lights.size(); i++) {
        const Light &l=lights[i];

        Point2f p4[4];
        l.rect.points(p4);
        vector<Point> poly(p4, p4 + 4);

        //画框、中心点、编号
        polylines(canva, poly, true, Scalar(0,0,255), 2);
        circle(canva, l.center, 3, Scalar(0,255,0), -1);
        putText(canva, to_string(i),
                l.center + Point2f(-14, -l.long_side * 0.5f - 6),
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 1);
    }

}


bool can_pair(const Light &l1, const Light &l2) {
    //a.颜色必须一致
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

    //e.水平间距要合理（用灯条长度的倍数，不用固定像素）
    float dx = abs(l1.center.x - l2.center.x);
    if (dx < 1.5f * aver_long || dx > 6.0f * aver_long) {
        return false;
    }

    return true;
}



vector<pair<int, int>> get_pairlights(const vector<Light> &lights) {
    vector<pair<int, int>> pairlights;
    vector<bool> used(lights.size(), false);

    for (int i = 0; i < (int)lights.size(); i++) {
        if (used[i]) continue;
        const Light &l1 = lights[i];

        for (int j = i + 1; j < (int)lights.size(); j++) {
            if (used[j]) continue;
            const Light &l2 = lights[j];

            if (!can_pair(l1, l2)) continue;

            pairlights.emplace_back(i, j);   // 存下标
            used[i] = used[j] = true;
            break;
        }
    }
    return pairlights;
}


Armor::Armor( const Light &l1, const Light &l2,const Mat &image) {
    // 1.由8个灯条顶点确定最小外接矩形
    vector<Point2f> pts(8);
    l1.rect.points(&pts[0]);
    l2.rect.points(&pts[4]);

    RotatedRect pairlights_rect = minAreaRect(pts);

    // 2.把配对灯带的外接矩形信息装进Armor：获得装甲板--外接框 中心点 四角点(传入后由构造函数角点自动排序）
    rect    = pairlights_rect;
    center  = pairlights_rect.center;
    corners = get_rect_corners(pairlights_rect);

    // 3.确定装甲板颜色
    if (l1.color == l2.color) {
        color = l1.color;
    }
    else if (l1.color == "unknown") {
        color = l2.color;               // 若有一根灯颜色可以判断，则用能判出来的那根
    }
    else if (l2.color == "unknown") {
        color = l1.color;
    }
    else {
        color = "unknown";
    }

    // 4.确定有效性
    check_valid(image);

}


void delete_repeat_armors(vector<Armor> &armors) {
    vector<Armor> keep;

    for (const Armor &a : armors) {
        bool repeat = false;

        for (const Armor &k : keep) {
            float th = 0.3f * max(a.rect.size.width, a.rect.size.height);

            //x、y 都比阈值近，就算同一块
            if (abs(a.center.x - k.center.x) < th &&
                abs(a.center.y - k.center.y) < th) {
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

    // 2.让四个角点在图上显示编号
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
    float th = r.angle * (float)CV_PI / 180.f;
    Point2f wdir(cos(th), sin(th));    // size.width 那条边的方向
    Point2f hdir(-wdir.y, wdir.x);     // 垂直方向（向量转 90°）

    // 2.找更贴近原图水平的一边作为x轴基准
    bool use_w = abs(wdir.x) >= abs(hdir.x);

    // 3. a朝右，b朝下
    Point2f a = use_w ? wdir : hdir;
    if (a.x < 0) a = -a;                   // 强制朝右
    Point2f b(-a.y, a.x);              // b.y = a.x > 0，自动朝下

    // 4.从中心出发，各走两个半步前往四个角
    array<Point2f, 4> out;
    float pa = (use_w ? r.size.width  : r.size.height) * 0.5f;
    float pb = (use_w ? r.size.height : r.size.width ) * 0.5f;
    out[0] = r.center - a * pa - b * pb;   // 左上
    out[1] = r.center + a * pa - b * pb;   // 右上
    out[2] = r.center + a * pa + b * pb;   // 右下
    out[3] = r.center - a * pa + b * pb;   // 左下
    return out;
}



void Armor::check_valid(const Mat &image) {
    valid = false;

    //1.四个角必须在图像内
    for (int i = 0; i < 4; i++) {
        if (corners[i].x < 0 || corners[i].x >= image.cols ||
            corners[i].y < 0 || corners[i].y >= image.rows) {
            return;
            }
    }

    //2.四边形面积不能为0
    if (rect.size.width * rect.size.height < 1.0f) {
        return;
    }

    //3.形状要像一块装甲板：比对长宽比
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
    //把旋转矩形的角度转成弧度，并定义w,h两个方向上的单位向量
    float th = r.angle * (float)CV_PI / 180.f;
    Point2f wdir(cos(th), sin(th));
    Point2f hdir(-wdir.y, wdir.x);

    //哪条边更长，哪条就是长轴
    Point2f longdir = (r.size.width >= r.size.height) ? wdir : hdir;

    //统一让长轴指向"下"，消除"同一方向有两种表示"
    if (longdir.y < 0) longdir = -longdir;

    return atan2(longdir.x, longdir.y) * 180.f / (float)CV_PI;
}