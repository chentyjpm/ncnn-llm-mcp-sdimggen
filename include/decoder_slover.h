#pragma once
#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <stack>
#include <fstream>
#include <map>
#include <cmath>
#include <net.h>
#include <algorithm>
#include <ctime>

using namespace std;

class DecodeSlover
{
public:
    DecodeSlover(int h, int w);
    DecodeSlover(int h, int w, const std::string& assets_dir);

    ncnn::Mat decode(ncnn::Mat sample);

private:
    void generate_param(int height, int width);

    const float factor[4] = { 5.48998f, 5.48998f, 5.48998f, 5.48998f };

    const float _mean_[3] = { -1.0f, -1.0f, -1.0f };
    const float _norm_[3] = { 127.5f, 127.5f, 127.5f };

    ncnn::Net net;

    std::string assets_dir_ = "assets";
};
