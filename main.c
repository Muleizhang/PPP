//! 由于需要自定义显示，本程序需要将e1.c与e1.h中
//! static void e1_ht16k33_chr_set(i2c_slave_info info, unsigned char bit,
//! unsigned char chr, unsigned char point)
//! 改为
//! void e1_ht16k33_chr_set(i2c_slave_info info, unsigned char bit, unsigned
//! char chr, unsigned char point)

#include "delay.h"
#include "e1.h"
#include "e2.h"
#include "e3.h"
#include "s1.h"
#include "s2.h"
#include "s5.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TIME_LIMIT 1000 // 游戏时间限制

// 1. 数码管显示

// 数码管段码定义
#define SEG_A (1 << 0)
#define SEG_B (1 << 1)
#define SEG_C (1 << 2)
#define SEG_D (1 << 3)
#define SEG_E (1 << 4)
#define SEG_F (1 << 5)
#define SEG_G (1 << 6)
#define SEG_DP (1 << 7)

// 数码管地址定义
static const uint8_t TUBE_ADDR[4][2] = {
    {0x02, 0x03}, // bit 1
    {0x04, 0x05}, // bit 2
    {0x06, 0x07}, // bit 3
    {0x08, 0x09}, // bit 4
};

// 数码管数字段码定义
const static unsigned char NUM_CODE[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,         // 0
    SEG_B | SEG_C,                                         // 1
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                 // 2
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                 // 3
    SEG_B | SEG_C | SEG_F | SEG_G,                         // 4
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                 // 5
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,         // 6
    SEG_A | SEG_B | SEG_C,                                 // 7
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G, // 8
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,         // 9
};

/**
 * @brief  这是一个自定义的数码管显示函数，可以显示单个数码管
 * @param  info     I2C 从设备信息结构体
 * @param  bit      数码管位（1~4）
 * @param  seg_mask 段选择掩码（比如 SEG_A|SEG_B|SEG_DP）
 * @retval 无
 * @note   可任意组合段，见SEG_A等定义
 */
void e1_tube_bit_set(i2c_slave_info info, int bit, uint8_t seg_mask)
{
  // asd
  unsigned char low = 0, high = 0;
  if (seg_mask & SEG_A)
    low |= 0x08;
  if (seg_mask & SEG_B)
    low |= 0x10;
  if (seg_mask & SEG_C)
    low |= 0x20;
  if (seg_mask & SEG_D)
    low |= 0x40;
  if (seg_mask & SEG_E)
    low |= 0x80;
  if (seg_mask & SEG_F)
    high |= 0x01;
  if (seg_mask & SEG_G)
    high |= 0x02;
  if (seg_mask & SEG_DP)
    high |= 0x04;

  if (bit >= 1 && bit <= 4)
  {
    i2c_reg_byte_write(info, TUBE_ADDR[bit - 1][0], low);
    i2c_reg_byte_write(info, TUBE_ADDR[bit - 1][1], high);
    i2c_byte_write(info, 0x81); // 更新显示
  }
}

/**
 * @brief  这是一个自定义的数码管显示函数，可以显示4个数码管
 * @param  info     I2C 从设备信息结构体
 * @param  seg_mask 段选择掩码数组（比如 {SEG_A|SEG_B|SEG_DP,
 * SEG_A|SEG_B|SEG_DP, SEG_A|SEG_B|SEG_DP, SEG_A|SEG_B|SEG_DP}）
 * @retval 无
 * @note   可任意组合段，见SEG_A等定义
 */
void e1_tube_all_set(i2c_slave_info info, uint8_t *seg_mask)
{
  for (int i = 0; i < 4; i++)
  {
    unsigned char low = 0, high = 0;
    if (seg_mask[i] & SEG_A)
      low |= 0x08;
    if (seg_mask[i] & SEG_B)
      low |= 0x10;
    if (seg_mask[i] & SEG_C)
      low |= 0x20;
    if (seg_mask[i] & SEG_D)
      low |= 0x40;
    if (seg_mask[i] & SEG_E)
      low |= 0x80;
    if (seg_mask[i] & SEG_F)
      high |= 0x01;
    if (seg_mask[i] & SEG_G)
      high |= 0x02;
    if (seg_mask[i] & SEG_DP)
      high |= 0x04;

    i2c_reg_byte_write(info, TUBE_ADDR[i][0], low);
    i2c_reg_byte_write(info, TUBE_ADDR[i][1], high);
  }
  i2c_byte_write(info, 0x81); // 更新显示
}

/**
 * @brief 跑马灯效果，从右边推进，窗口4位，支持小数点
 * @param info I2C 设备信息
 * @param str  原始字符串（如 "Welcome-to-bpu"）
 * @param offset 当前窗口偏移（0 ~ total_window_len-1）
 */
void e1_tube_marquee_display(i2c_slave_info info, const char *str, int offset)
{
  int str_len = strlen(str);
  int window = 4;

  // 只在前面补window个'-'
  char pad_str[64] = {0};
  memset(pad_str, '-', window);  // 前补
  strcpy(pad_str + window, str); // 中间
  // 不补后缀'-'

  char disp_buf[8] = {0};
  int buf_i = 0;

  for (int i = 0; i < window && buf_i < 7; i++)
  {
    disp_buf[buf_i++] = pad_str[offset + i];
    // 支持小数点
    if (pad_str[offset + i + 1] == '.' && buf_i < 7)
    {
      disp_buf[buf_i++] = '.';
      i++;
    }
  }
  disp_buf[buf_i] = '\0';

  e1_tube_str_set(info, disp_buf);
}

/**
 * @brief 加载动画，从左到右依次点亮数码管
 * @param info I2C 设备信息
 * @param round 轮数
 */
void loading(i2c_slave_info info, int round)
{
  uint8_t nothing[4] = {0, 0, 0, 0}; // 初始化段掩码数组
  int delay = 60;                    // 延时60ms
  while (round--)
  {
    e1_tube_bit_set(info, 1, SEG_A);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 2, SEG_A);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 3, SEG_A);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 4, SEG_A);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 4, SEG_B);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 4, SEG_C);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 4, SEG_D);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 3, SEG_D);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 2, SEG_D);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 1, SEG_D);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 1, SEG_E);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);

    e1_tube_bit_set(info, 1, SEG_F);
    delay_ms(delay);
    e1_tube_all_set(info, nothing);
  }
}

/**
 * @brief 将HSV颜色转换为RGB颜色
 * @param h 色相（0-360度）
 * @param s 饱和度（0-255）
 * @param v 明度（0-255）
 * @param r 输出红色分量（0-255）
 * @param g 输出绿色分量（0-255）
 * @param b 输出蓝色分量（0-255）
 */
void HSV2RGB(int h, int s, int v, unsigned char *r, unsigned char *g,
             unsigned char *b)
{
  int i = h / 60;
  int f = h % 60;
  int p = v * (255 - s) / 255;
  int q = v * (255 - s * f / 60) / 255;
  int t = v * (255 - s * (60 - f) / 60) / 255;
  switch (i)
  {
  case 0:
    *r = v;
    *g = t;
    *b = p;
    break;
  case 1:
    *r = q;
    *g = v;
    *b = p;
    break;
  case 2:
    *r = p;
    *g = v;
    *b = t;
    break;
  case 3:
    *r = p;
    *g = q;
    *b = v;
    break;
  case 4:
    *r = t;
    *g = p;
    *b = v;
    break;
  case 5:
  default:
    *r = v;
    *g = p;
    *b = q;
    break;
  }
}

/**
 * @brief 欢迎界面，彩灯和数码管跑马灯
 * @param tube_info 数码管信息
 * @param led_info 彩灯信息
 * @param key_info 按键信息
 */
void welcome(i2c_slave_info tube_info, i2c_slave_info led_info,
             i2c_slave_info key_info)
{
  const char *msg = "Welcome-to-PPP2025----";
  int window = 4;
  int msg_len = 15;
  int total_steps = msg_len + window;
  int offset = 0;

  unsigned char r, g, b;
  int color_steps = 10;                // 200ms内彩灯变色次数
  int color_delay = 200 / color_steps; // 每次变色间隔ms

  int hue_base = 0;

  for (int i = 0;; i++)
  {
    e1_tube_marquee_display(tube_info, msg, offset);

    // 这200ms内彩灯快速变color_steps次
    for (int j = 0; j < color_steps; j++)
    {
      int hue = (hue_base + j * (360 / color_steps)) % 360;
      HSV2RGB(hue, 255, 128, &r, &g, &b);
      e1_led_rgb_set(led_info, r, g, b);
      delay_ms(color_delay);
      if (s1_key_value_get(key_info) != 0)
      {
        e1_led_rgb_set(led_info, 0, 0, 0); // 熄灭
        e1_tube_str_set(tube_info, "");    // 显示结束信息
        return;
      }
    }
    hue_base = (hue_base + 30) % 360; // 每步整体推进色相
    offset = (offset + 1) % total_steps;
  }
}

// 2. 按键

// 双按键器结构体
typedef struct
{
  i2c_slave_info key1;
  i2c_slave_info key2;
  int count;
} dual_key_info;

/**
 * @brief 初始化双按键器，返回双按键器结构体
 * @param 无
 * @retval 双按键器结构体
 * @note   双按键器结构体包含两个按键器信息和计数器
 */
dual_key_info s1_multi_key_init(void)
{
  dual_key_info dual_info;
  dual_info.count = 0;
// S1按键传感器地址
#if defined(GD32F450) || defined(GD32F470)
  const static unsigned char S1_HT16K33_ADDR[] = {0xE8, 0xEA, 0xEC, 0xEE};
#else
  const static unsigned char S1_HT16K33_ADDR[] = {0x74, 0x75, 0x76, 0x77};
#endif
  i2c_init();
  for (int i = 0; i < sizeof(I2C_PERIPH_NUM) / sizeof(unsigned int); i++)
  {
    for (int j = 0; j < sizeof(S1_HT16K33_ADDR) / sizeof(unsigned char); j++)
    {
      i2c_slave_info info =
          i2c_slave_detect(I2C_PERIPH_NUM[i], S1_HT16K33_ADDR[j]);
      if (info.flag && dual_info.count < 2)
      {
        // 初始化传感器（按照官方驱动）
        i2c_byte_write(info, 0x21);

        // 存储到对应位置
        if (dual_info.count == 0)
        {
          dual_info.key1 = info;
        }
        else if (dual_info.count == 1)
        {
          dual_info.key2 = info;
        }
        dual_info.count++;
      }
    }
  }
  return dual_info;
}

/**
 * @brief 分别获取两个玩家的按键值
 * @param dual_info 双传感器结构体
 * @param player 玩家编号（1或2）
 * @retval 按键值
 * @note   如果玩家编号大于双传感器数量，返回SWN
 */
char get_player_key(dual_key_info dual_info, int player)
{
  if (player == 1 && dual_info.count >= 1)
  {
    return s1_key_value_get(dual_info.key1);
  }
  else if (player == 2 && dual_info.count >= 2)
  {
    return s1_key_value_get(dual_info.key2);
  }
  return SWN;
}

/**
 * @brief 测试按键
 */
void test_button()
{
  i2c_slave_info s1_key = s1_key_init();
  i2c_slave_info e1_tube = e1_tube_init();
  char str[8]; // 足够大

  char i = s1_key_value_get(s1_key); // 读取按键值
  sprintf(str, "%d", i);             // 转成字符串

  e1_tube_str_set(e1_tube, str); // 显示
}

// 3. NFC

// 卡ID
const unsigned char CARD0_ID[4] = {0x93, 0x71, 0xAF, 0x95}; // card0: 9371AF95
const unsigned char CARD1_ID[4] = {0x63, 0x93, 0xBE, 0x95}; // card1: 6393BE95
// 卡类型
unsigned char CardType[2] = {0};
// 识别到的卡ID
unsigned char CardID[4] = {0};

// 比较两个卡ID是否一致
int compare_card_id(const unsigned char *id1, const unsigned char *id2)
{
  for (int i = 0; i < 4; i++)
  {
    if (id1[i] != id2[i])
      return 0;
  }
  return 1;
}

/**
 * @brief 返回当前卡号：0=card0，1=card1，-1=没有卡，-2=不是这两张卡
 * @param s5_nfc NFC信息
 * @retval 当前卡号
 */
int get_current_card_number(i2c_slave_info s5_nfc)
{
  if (s5_nfc_request(s5_nfc, PICC_REQIDL, CardType) == MI_OK &&
      s5_nfc_anticoll(s5_nfc, CardID) == MI_OK)
  {
    if (compare_card_id(CardID, CARD0_ID))
    {
      return 0;
    }
    else if (compare_card_id(CardID, CARD1_ID))
    {
      return 1;
    }
    else
    {
      return -2;
    }
  }
  return -1;
}

/**
 * @brief 测试NFC
 * @param e1_tube 数码管信息
 * @param e1_led 彩灯信息
 * @param s1_key 按键信息
 * @param s5_nfc NFC信息
 */
void nfc_test(i2c_slave_info e1_tube, i2c_slave_info e1_led,
              i2c_slave_info s1_key, i2c_slave_info s5_nfc)
{
  unsigned char CardID[4] = {0};
  char buf[10] = {0};

  e1_tube_str_set(e1_tube, "nfc");
  delay_ms(500);

  while (1)
  {
    int pos = 0;
    if (s1_key_value_get(s1_key) != 0)
    {
      pos = 1;
    }
    if (s5_nfc_request(s5_nfc, 0x26, NULL) == MI_OK &&
        s5_nfc_anticoll(s5_nfc, CardID) == MI_OK)
    {
      sprintf(buf, "%02x%02x", CardID[0 + 2 * pos], CardID[1 + 2 * pos]);
      e1_tube_str_set(e1_tube, buf);
      if (compare_card_id(CardID, CARD0_ID))
      {
        e1_led_rgb_set(e1_led, 0, 100, 0);
      }
      else if (compare_card_id(CardID, CARD1_ID))
      {
        e1_led_rgb_set(e1_led, 0, 0, 100);
      }
      else
      {
        e1_led_rgb_set(e1_led, 100, 100, 0);
      }
    }
    else
    {
      e1_led_rgb_set(e1_led, 100, 100, 0);
    }
  }
}

// 4. 游戏代码

// 4.1 游戏代码与显示

// 游戏代码结构体定义
struct game_code
{
  int fan;
  int fan_unsolved;
  int tube_1;
  int tube_2;
  int tube_3;
  int unsolved;
  int oops;
};

/**
 * @brief 显示当前游戏代码状态
 * @param tube_info 数码管信息
 * @param fan_info 风扇信息
 * @param code 游戏代码
 */
void display_code(i2c_slave_info tube_info, i2c_slave_info fan_info,
                  struct game_code code)
{
  // 显示风扇
  if (code.fan == 0)
  {
    e2_fan_speed_set(fan_info, 0);
  }
  else
  {
    e2_fan_speed_set(fan_info, 100);
  }
  uint8_t seg_mask[4] = {0};

  // 处理所有tube值，如果有相同的%3值，则在同一位置累加段掩码
  int tubes[3] = {code.tube_1, code.tube_2, code.tube_3};

  for (int tube_idx = 0; tube_idx < 3; tube_idx++)
  {
    if (tubes[tube_idx] > 0) // 只处理非零值
    {
      int position = (tubes[tube_idx] - 1) % 3 + 1; // 数码管位置 (1-3)
      int segment_type = (tubes[tube_idx] - 1) / 3; // 段类型 (0-2)

      // 根据段类型设置对应的段，使用按位或来合并
      switch (segment_type)
      {
      case 0:
        seg_mask[position] |= SEG_A;
        break;
      case 1:
        seg_mask[position] |= SEG_G;
        break;
      case 2:
        seg_mask[position] |= SEG_D;
        break;
      }
    }
  }
  // 显示unsolved
  seg_mask[0] = NUM_CODE[code.unsolved] + SEG_DP;
  e1_tube_all_set(tube_info, seg_mask);
}

// 4.2 游戏代码随机生成

/**
 * @brief 生成随机数 1~1000
 * @param temp_humi_info 温湿度传感器信息
 * @retval 随机数
 * @note   使用温度和湿度生成一个伪随机数
 */
int random(i2c_slave_info temp_humi_info)
{
  s2_ths_t t = s2_ths_value_get(temp_humi_info);
  // 使用温度和湿度生成一个伪随机数
  int seed = (int)(t.temp * 100) + (int)(t.humi * 100);
  seed = (seed * 1103515245 + 12345) & 0x7fffffff; // 线性同余生成器
  return seed % 1000;                              // 返回0-99之间的伪随机数
}

/**
 * @brief 随机生成游戏代码
 * @param temp_humi_info 温湿度传感器信息
 * @param code 游戏代码
 */
void random_game_code(i2c_slave_info temp_humi_info, struct game_code *code)
{
  int random_num = random(temp_humi_info) % 1000; // 确保在0-999之间

  code->fan = random_num % 2; // 随机风扇状态
  code->fan_unsolved = 1;
  code->tube_1 = random_num / 100;     // 随机管道编号
  code->tube_2 = random_num / 10 % 10; // 随机管道编号
  code->tube_3 = random_num % 10;      // 随机管道编号

  // 保证tube_1, tube_2, tube_3 不重复
  while (code->tube_1 == code->tube_2 || code->tube_1 == code->tube_3 ||
         code->tube_2 == code->tube_3)
  {
    int random_num = random(temp_humi_info) % 1000; // 确保在0-999之间

    code->fan = random_num % 2;          // 随机风扇状态
    code->tube_1 = random_num / 100;     // 随机管道编号
    code->tube_2 = random_num / 10 % 10; // 随机管道编号
    code->tube_3 = random_num % 10;      // 随机管道编号
  }
  code->unsolved = 1 + (code->tube_1 > 0) + (code->tube_2 > 0) +
                   (code->tube_3 > 0); // 随机未解答数量
  code->oops = 0;
}

// 多人游戏专用的游戏代码生成（无风扇和NFC）
void random_multi_game_code(i2c_slave_info temp_humi_info,
                            struct game_code *code)
{
  int random_num = random(temp_humi_info) % 1000; // 确保在0-999之间

  // 多人模式不使用风扇和NFC
  code->fan = 0;
  code->fan_unsolved = 0;
  code->tube_1 = random_num / 100;     // 随机管道编号
  code->tube_2 = random_num / 10 % 10; // 随机管道编号
  code->tube_3 = random_num % 10;      // 随机管道编号

  // 保证tube_1, tube_2, tube_3 不重复
  while (code->tube_1 == code->tube_2 || code->tube_1 == code->tube_3 ||
         code->tube_2 == code->tube_3)
  {
    int random_num = random(temp_humi_info) % 1000; // 确保在0-999之间
    code->tube_1 = random_num / 100;                // 随机管道编号
    code->tube_2 = random_num / 10 % 10;            // 随机管道编号
    code->tube_3 = random_num % 10;                 // 随机管道编号
  }
  code->unsolved = (code->tube_1 > 0) + (code->tube_2 > 0) +
                   (code->tube_3 > 0); // 只计算tube数量
  code->oops = 0;
}

// 4.3 游戏核心函数

/**
 * @brief 保证score不小于0 不大于100
 * @param score 分数
 * @param add 增加的分数
 */
void score_add(int *score, int add)
{
  if (*score + add < 0) // 如果score+add小于0，则score=0
  {
    *score = 0;
  }
  else if (*score + add > 100) // 如果score+add大于100，则score=100
  {
    *score = 100;
  }
  else // 否则score+add
  {
    *score += add;
  }
}

/**
 * @brief 初始化所有设备
 */
void init_all(i2c_slave_info e1_tube, i2c_slave_info e1_led,
              i2c_slave_info e2_fan, i2c_slave_info e3_curtain)
{
  e1_tube_str_set(e1_tube, "");
  e1_led_rgb_set(e1_led, 0, 0, 0);
  e2_fan_speed_set(e2_fan, 0);
  e3_curtain_position_set(e3_curtain, 100);
  loading(e1_tube, 1);
}

/**
 * @brief 选择模式(单人/多人/测试多按键)
 * @param e1_tube 数码管信息
 * @param e1_led 彩灯信息
 * @param s1_key 按键信息
 * @retval 模式
 */
int chose_mode(i2c_slave_info e1_tube, i2c_slave_info e1_led,
               i2c_slave_info s1_key)
{

  const char *msg = "CHOOSE-MODE----";
  int window = 4;
  int msg_len = 12;
  int total_steps = msg_len + window;
  int offset = 0;

  unsigned char r, g, b;
  int color_steps = 10;                // 200ms内彩灯变色次数
  int color_delay = 200 / color_steps; // 每次变色间隔ms

  int hue_base = 0;

  for (int i = 0;; i++)
  {
    e1_tube_marquee_display(e1_tube, msg, offset);

    // 这200ms内彩灯快速变color_steps次
    for (int j = 0; j < color_steps; j++)
    {
      int hue = (hue_base + j * (360 / color_steps)) % 360;
      HSV2RGB(hue, 255, 128, &r, &g, &b);
      e1_led_rgb_set(e1_led, r, g, b);
      delay_ms(color_delay);
      int key = s1_key_value_get(s1_key);
      if (key != 0)
      {
        e1_led_rgb_set(e1_led, 0, 0, 0); // 熄灭
        e1_tube_str_set(e1_tube, "");    // 显示结束信息
        if (key == '1')
        {
          return 1;
        }
        else if (key == '2')
        {
          return 2;
        }
        else if (key == '3')
        {
          return 3;
        }
      }
    }
    hue_base = (hue_base + 30) % 360; // 每步整体推进色相
    offset = (offset + 1) % total_steps;
  }
}

/**
 * @brief 单人游戏
 * @param e1_tube 数码管信息
 * @param e1_led 彩灯信息
 * @param e2_fan 风扇信息
 * @param e3_curtain 窗帘信息
 * @param s1_key 按键信息
 * @param s2_imu 惯性传感器信息
 * @param s2_temp_humi 温湿度传感器信息
 * @param s5_nfc NFC信息
 * @retval 轮数
 */
int solo_game(i2c_slave_info e1_tube, i2c_slave_info e1_led,
              i2c_slave_info e2_fan, i2c_slave_info e3_curtain,
              i2c_slave_info s1_key, i2c_slave_info s2_imu,
              i2c_slave_info s2_temp_humi, i2c_slave_info s5_nfc)
{
  int score = 100;
  int round = 0;
  struct game_code code;
  int delay_time = 200;

  // 主循环
  while (score > 0)
  {
    // 每次轮次
    round++;
    random_game_code(s2_temp_humi, &code);

    while (code.unsolved != 0 && score > 0)
    {
      // 显示tube、fan和unsolved
      display_code(e1_tube, e2_fan, code);
      e3_curtain_position_set(e3_curtain, score);

      // 按键被按下，检查是否击中地鼠
      int key = s1_key_value_get(s1_key);
      if (key != 0)
      {
        key = key - '0';
        if (key == code.tube_1)
        {
          score_add(&score, 5);
          e1_led_rgb_set(e1_led, 0, 255, 0);
          code.unsolved--;
          code.tube_1 = 0;
        }
        else if (key == code.tube_2)
        {
          score_add(&score, 5);
          e1_led_rgb_set(e1_led, 0, 255, 0);
          code.unsolved--;
          code.tube_2 = 0;
        }
        else if (key == code.tube_3)
        {
          score_add(&score, 5);
          e1_led_rgb_set(e1_led, 0, 255, 0);
          code.unsolved--;
          code.tube_3 = 0;
        }
        else
        {
          score_add(&score, -10);
          e1_led_rgb_set(e1_led, 255, 0, 0);
          e1_tube_str_set(e1_tube, "00P5"); // 显示错误信息
          delay_ms(50);
        }
      }

      // 检查nfc 是否是正确的卡片
      if (code.fan_unsolved == 1)
      {
        int card_number = get_current_card_number(s5_nfc);
        if (card_number == code.fan)
        {
          code.unsolved--;
          code.fan_unsolved = 0;
          e1_led_rgb_set(e1_led, 0, 255, 0);
        }
        else
        {
          score_add(&score, -1);
          e1_led_rgb_set(e1_led, 255, 0, 0);
        }
      }

      delay_ms(delay_time);
      e1_led_rgb_set(e1_led, 0, 0, 0);
    }
  }
  return round;
}

/**
 * @brief 多人游戏
 * @param e1_tube 数码管信息
 * @param e1_led 彩灯信息
 * @param e2_fan 风扇信息
 * @param e3_curtain 窗帘信息
 * @param s1_multi_key 多按键信息
 * @param s2_imu 惯性传感器信息
 * @param s2_temp_humi 温湿度传感器信息
 * @param s5_nfc NFC信息
 * @retval 轮数
 */
int multi_game(i2c_slave_info e1_tube, i2c_slave_info e1_led,
               i2c_slave_info e2_fan, i2c_slave_info e3_curtain,
               dual_key_info s1_multi_key, i2c_slave_info s2_imu,
               i2c_slave_info s2_temp_humi, i2c_slave_info s5_nfc)
{

  delay_ms(1000);
  int score = 50; // player2胜率，50=平衡，0=player1胜，100=player2胜
  struct game_code code;
  int delay_time = 200;

  // 主循环
  int round = 0;
  char key1 = 0, key2 = 0;
  while (score > 0 && score < 100)
  {
    round++;
    // 每次轮次生成新的游戏代码（无风扇和NFC）
    random_multi_game_code(s2_temp_humi, &code);

    while (code.unsolved != 0 && score > 0 && score < 100)
    {
      // 显示tube状态和当前score
      display_code(e1_tube, e2_fan, code);
      e3_curtain_position_set(e3_curtain, score);

      // 同时检查两个玩家的按键，确保公平
      // 偶数轮次，player1先按，奇数轮次，player2先按
      if (round % 2 == 0)
      {
        key1 = get_player_key(s1_multi_key, 1);
        key2 = get_player_key(s1_multi_key, 2);
      }
      else
      {
        key2 = get_player_key(s1_multi_key, 2);
        key1 = get_player_key(s1_multi_key, 1);
      }

      int player1_scored = 0, player2_scored = 0;

      // 处理player1按键
      if (key1 != SWN)
      {
        key1 = key1 - '0';
        if (key1 == code.tube_1)
        {
          score_add(&score, -5); // player1正确，score减少（对player2不利）
          player1_scored = 1;
          code.tube_1 = 0;
          code.unsolved--;
        }
        else if (key1 == code.tube_2)
        {
          score_add(&score, -5);
          player1_scored = 1;
          code.tube_2 = 0;
          code.unsolved--;
        }
        else if (key1 == code.tube_3)
        {
          score_add(&score, -5);
          player1_scored = 1;
          code.tube_3 = 0;
          code.unsolved--;
        }
        else
        {
          score_add(&score, 3); // player1错误，score增加（对player2有利）
          player1_scored = -1;
        }
      }

      // 处理player2按键
      if (key2 != SWN)
      {
        key2 = key2 - '0';
        if (key2 == code.tube_1 && code.tube_1 != 0) // 确保目标还存在
        {
          score_add(&score, 5); // player2正确，score增加（对player2有利）
          player2_scored = 1;
          code.tube_1 = 0;
          code.unsolved--;
        }
        else if (key2 == code.tube_2 && code.tube_2 != 0)
        {
          score_add(&score, 5);
          player2_scored = 1;
          code.tube_2 = 0;
          code.unsolved--;
        }
        else if (key2 == code.tube_3 && code.tube_3 != 0)
        {
          score_add(&score, 5);
          player2_scored = 1;
          code.tube_3 = 0;
          code.unsolved--;
        }
        else
        {
          score_add(&score, -3); // player2错误，score减少（对player2不利）
          player2_scored = -1;
        }
      }

      // 根据得分情况设置LED颜色
      if (player1_scored == 1 && player2_scored == 1)
      {
        e1_led_rgb_set(e1_led, 255, 255, 255); // 白色：双方都得分
      }
      else if (player1_scored == 1)
      {
        e1_led_rgb_set(e1_led, 0, 255, 0); // 绿色：player1得分
      }
      else if (player2_scored == 1)
      {
        e1_led_rgb_set(e1_led, 0, 0, 255); // 蓝色：player2得分
      }
      else if (player1_scored == -1 && player2_scored == -1)
      {
        e1_led_rgb_set(e1_led, 255, 0, 255); // 紫色：双方都失分
      }
      else if (player1_scored == -1)
      {
        e1_led_rgb_set(e1_led, 255, 0, 0); // 红色：player1失分
      }
      else if (player2_scored == -1)
      {
        e1_led_rgb_set(e1_led, 255, 255, 0); // 黄色：player2失分
      }

      delay_ms(delay_time);
      e1_led_rgb_set(e1_led, 0, 0, 0); // 熄灭LED
    }
  }

  // 返回获胜玩家
  if (score <= 50)
  {
    return 1; // player1胜利
  }
  else
  {
    return 2; // player2胜利
  }
}

/**
 * @brief 主函数
 * @param 无
 * @retval 无
 * @note   初始化所有设备，进入欢迎界面，选择模式，进入游戏
 */
int main()
{

  // init
  i2c_slave_info e1_tube = e1_tube_init();
  i2c_slave_info e1_led = e1_led_init();
  i2c_slave_info e2_fan = e2_fan_init();
  i2c_slave_info e3_curtain = e3_curtain_init();
  i2c_slave_info s1_key = s1_key_init();
  i2c_slave_info s2_imu = s2_imu_init();
  i2c_slave_info s2_temp_humi = s2_ths_init();
  i2c_slave_info s5_nfc = s5_nfc_init();

  // 如果按键被按下，则进入nfc测试模式
  if (s1_key_value_get(s1_key) != 0)
  {
    e1_led_rgb_set(e1_led, 100, 100, 0);
    nfc_test(e1_tube, e1_led, s1_key, s5_nfc);
  }
  // 否则进入游戏模式
  // init
  while (1)
  {
    init_all(e1_tube, e1_led, e2_fan, e3_curtain);
    welcome(e1_tube, e1_led, s1_key);
    delay_ms(1000);
    int mode = chose_mode(e1_tube, e1_led, s1_key);
    if (mode == 1)
    {
      e1_tube_str_set(e1_tube, "SOLO");
      delay_ms(1000);
      int round = solo_game(e1_tube, e1_led, e2_fan, e3_curtain, s1_key, s2_imu,
                            s2_temp_humi, s5_nfc);
      char round_str[8];
      sprintf(round_str, "%d", round);
      e1_tube_str_set(e1_tube, round_str);
      delay_ms(2000);
    }
    else if (mode == 2)
    {
      e1_tube_str_set(e1_tube, "MULT");
      delay_ms(1000);
      dual_key_info s1_multi_key = s1_multi_key_init();
      if (s1_multi_key.count != 2)
      {
        e1_tube_str_set(e1_tube, "ERR");
        e1_led_rgb_set(e1_led, 255, 0, 0);
        delay_ms(1000);
        continue;
      }

      int winner = multi_game(e1_tube, e1_led, e2_fan, e3_curtain, s1_multi_key,
                              s2_imu, s2_temp_humi, s5_nfc);
      if (winner == 1)
      {
        e1_led_rgb_set(e1_led, 0, 255, 0);
        e1_tube_str_set(e1_tube, "P1");
      }
      else if (winner == 2)
      {
        e1_led_rgb_set(e1_led, 0, 0, 255);
        e1_tube_str_set(e1_tube, "P2");
      }
      delay_ms(2000);
    }
    else if (mode == 3)
    {
      dual_key_info s1_multi_key = s1_multi_key_init();
      if (s1_multi_key.count != 2)
      {
        e1_tube_str_set(e1_tube, "ERR");
        e1_led_rgb_set(e1_led, 255, 0, 0);
        delay_ms(1000);
        continue;
      }

      while (1)
      {
        char key = s1_key_value_get(s1_multi_key.key1);
        if (key != 0)
        {
          e1_led_rgb_set(e1_led, 0, 255, 0);
          e1_tube_str_set(e1_tube, &key);
        }
        key = s1_key_value_get(s1_multi_key.key2);
        if (key != 0)
        {
          e1_led_rgb_set(e1_led, 0, 0, 255);
          e1_tube_str_set(e1_tube, &key);
        }
        delay_ms(200);
      }
    }
  }
}
