#include <windows.h>
#include "armor.h"
#include <filesystem>


int main(int argc,char *argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    //输入路径：命令行优先，默认用项目内相对路径
    string imagePath = (argc > 1) ? argv[1] : "images/outpost_frame.png";

    //创建output目录（如果不存在的话）
    filesystem::create_directories("output");




    //1.读图并检查
    Mat input = imread(imagePath);
    if (input.empty()) {
        cerr << "图片读取失败：" << imagePath << endl;
        cerr << "请确认文件存在，且路径相对于程序运行目录。" << endl;
        return 1;
    }
    cout << "width x height:" << input.cols << " x " << input.rows << ", channels=" << input.channels() << endl;
    namedWindow("image", WINDOW_FREERATIO);
    imshow("image", input);
    imwrite("output/input.png",input);




    //2.单帧检测
    Photo demo;
    Mat image=demo.flipped(input);

    DetectDebug debug;
    vector<Armor> armors = detect_armors(image,&debug);




    //3.保存调试图
    imwrite("output/target_mask.png", debug.mask);
    imwrite("output/mask_close.png",  debug.mask_morphology);

    Mat lights_canva = image.clone();
    demo.draw_lights(lights_canva, debug.lights);
    imwrite("output/lights.png", lights_canva);



    
    //4.画图、打印
    Mat canva = image.clone();

    for (const Armor &armor : armors) {
        demo.draw_armor(canva, armor);
        armor.print_information();
    }

    if (armors.empty()) {
        cout << "未检测到装甲板" << endl;
    }

    imwrite("output/result.png", canva);
    namedWindow("result", WINDOW_FREERATIO);
    imshow("result", canva);

    waitKey(0);
    return 0;
}