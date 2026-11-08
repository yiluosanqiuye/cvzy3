#include <windows.h>
#include "zy3.h"
#include <filesystem>


int main(int argc,char *argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    //输入路径：命令行优先，默认用项目内相对路径
    string imgPath = (argc > 1) ? argv[1] : "images/outpost_frame.png";

    //确保output目录存在,避免imwrite静默失败
    filesystem::create_directories("output");

    //1.读图并检查
    Mat input = imread(imgPath);
    if (input.empty()) {
        cerr << "图片读取失败：" << imgPath << endl;
        cerr << "请确认文件存在，且路径相对于程序运行目录。" << endl;
        return 1;
    }
    cout << "width x height:" << input.cols << " x " << input.rows << ", channels=" << input.channels() << endl;
    namedWindow("image", WINDOW_FREERATIO);
    imshow("image", input);
    imwrite("output/input.png",input);

    //2.mask
    Photo demo;

    Mat image=demo.flipped(input);
    Mat mask=demo.get_mask(image);
    Mat mask_morphology=demo.mask_morphology(mask);

    //3.灯条
    vector<Light> lights=demo.get_lights(mask_morphology,image);
    Mat light_canva=image.clone();
    demo.draw_lights(light_canva,lights);
    imwrite("output/lights.png",light_canva);

    //4.灯条配对
    vector<pair<int,int>> pairlights=get_pairlights(lights);

    //5.装甲板
    vector<Armor> armors;

    for (const auto &p : pairlights) {
        Armor armor(lights[p.first], lights[p.second], image);

        if (lights[p.first].center.x <= lights[p.second].center.x) {
            armor.left_light_id  = p.first;
            armor.right_light_id = p.second;
        }
        else {
            armor.left_light_id  = p.second;
            armor.right_light_id = p.first;
        }

        armors.push_back(armor);
    }

    delete_repeat_armors(armors);

    //6.画图、打印数据
    Mat canva = image.clone();

    for (const Armor &armor : armors) {
        demo.draw_armor(canva, armor);

        cout << "===== 装甲板 =====" << endl;
        cout << "颜色：" << armor.color << endl;
        cout << "左灯条：编号" << armor.left_light_id
             << "   右灯条：编号" << armor.right_light_id << endl;
        cout << "外接框：" << armor.rect.size.width << " x " << armor.rect.size.height
             << "  角度：" << armor.rect.angle << "°" << endl;
        cout << "中心：(" << armor.center.x << ", " << armor.center.y << ")" << endl;
        cout << "角点：";
        for (int i = 0; i < 4; i++) {
            cout << i << "(" << armor.corners[i].x << ","
                 << armor.corners[i].y << ") ";
        }
        cout << endl;
        cout << "目标" << (armor.valid ? "有效" : "无效") << endl;
    }
    namedWindow("result",WINDOW_FREERATIO);
    imshow("result",canva);
    imwrite("output/result.png",canva);



    waitKey(0);

    return 0;
}