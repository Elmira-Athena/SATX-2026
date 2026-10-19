#include <16F877A.h>
#include <string.h>
#include <stdlib.h>
#fuses HS,NOWDT,NOBROWNOUT, NOLVP
#use delay(crystal=20MHz)
#use rs232(baud=9600, xmit=PIN_C6, rcv=PIN_C7, stream=PC_UART)
#define MAX30102_ADDR 0xAE
#use i2c(MASTER, SDA=PIN_C4, SCL=PIN_C3, FAST = 100000, FORCE_HW)
// LCD
#define LCD_ENABLE_PIN  PIN_C5
#define LCD_RS_PIN      PIN_D2
#define LCD_RW_PIN      PIN_D3
#define LCD_DATA4       PIN_D4
#define LCD_DATA5       PIN_D5
#define LCD_DATA6       PIN_D6
#define LCD_DATA7       PIN_D7
#include <lcd.c>
// Keypad
#define R1 PIN_B0
#define R2 PIN_B1
#define R3 PIN_B2
#define R4 PIN_B3
#define C1 PIN_B4
#define C2 PIN_B5
#define C3 PIN_B6
#define C4 PIN_B7
// HC-SR04
#define TRIG PIN_D0
#define ECHO PIN_D1
// HX711
#define HX711_DT   PIN_C0
#define HX711_SCK  PIN_C1
#define LED1 PIN_E0
#define LED2 PIN_A4
#define LED3 PIN_A5
// ================= BIEN TOAN CUC =================
unsigned int32 ms_count = 0;
unsigned int32 start_time = 0;
int1 measuring = 0;
int1 request_print = 0;
unsigned int32 red_raw, ir_raw;
unsigned int16 beat_count = 0;
unsigned int32 dc_ir = 0, dc_red = 0;
signed int32 ac_ir, ac_red;
signed int32 prev_ac_ir = 0;
float bpm = 0, spo2 = 0;
float ac_red_sum = 0, ac_ir_sum = 0;
unsigned int32 sample_count = 0;
unsigned int32 last_sample_ms = 0;
float h = 0, m = 0;
float bmi = 0;
float hx711_offset = 0;
float hx711_scale = 2852.35f;
char CMD_DO[3] = {'D','o','\0'};
char CMD_LAMMOI[8] = {'L','a','m',' ','m','o','i','\0'};
char key, cmd[10];
unsigned int32 warmup_ms = 2000;
int1 warmed_up = 0;
unsigned int32 last_peak_ms = 0;
unsigned int32 finger_change_ms = 0;
int1 finger_present = 0;
// ================= TIMER2 =================
#int_TIMER2
void timer2_isr() {
   ms_count++;
}
// ================= KEYPAD =================
char keypad_getkey() {
   char keymap[4][4] = {
      {'1','2','3','A'},
      {'4','5','6','B'},
      {'7','8','9','C'},
      {'*','0','#','D'}
   };
   int rows[4] = {R1, R2, R3, R4};
   int cols[4] = {C1, C2, C3, C4};
   int r, c;
   for(r = 0; r < 4; r++) {
      output_high(R1);
      output_high(R2);
      output_high(R3);
      output_high(R4);
      output_low(rows[r]);
      delay_us(10);
      for(c = 0; c < 4; c++) {
         if(!input(cols[c])) {
            delay_ms(20);
            if(!input(cols[c])) {
               return keymap[r][c];
            }
         }
      }
   }
   return 0;
}

// ================= MAX30102 =================
int1 max30102_write(byte reg, byte val) {
   int1 ack;
   i2c_start();
   ack = i2c_write(MAX30102_ADDR);
   i2c_write(reg);
   i2c_write(val);
   i2c_stop();
   return ack;
}

void max30102_read_fifo(unsigned int32 *red, unsigned int32 *ir) {
   byte d[6];

   i2c_start();
   i2c_write(MAX30102_ADDR);
   i2c_write(0x07);

   i2c_start();
   i2c_write(MAX30102_ADDR | 1);

   for(int i = 0; i < 5; i++)
      d[i] = i2c_read(1);
   d[5] = i2c_read(0);

   i2c_stop();

   *red = ((unsigned int32)d[0]<<16 | (unsigned int32)d[1]<<8 | d[2]) & 0x03FFFF;
   *ir  = ((unsigned int32)d[3]<<16 | (unsigned int32)d[4]<<8 | d[5]) & 0x03FFFF;
}

int1 max30102_init() {
   max30102_write(0x09, 0x40);
   delay_ms(100);
   max30102_write(0x06, 0x0F);
   max30102_write(0x09, 0x03);
   max30102_write(0x0A, 0x47);
   max30102_write(0x0C, 0x4F);
   max30102_write(0x0D, 0x4F);
   return 1;
}

// ================= XU LÝ PPG =================
void process_data(unsigned int32 r, unsigned int32 i) {
   dc_red = (dc_red * 31 + r) >> 5;
   dc_ir = (dc_ir * 31 + i) >> 5;

   ac_red = (signed int32)r - dc_red;
   ac_ir = (signed int32)i - dc_ir;

   if(!warmed_up) {
      if(ms_count - start_time < warmup_ms) {
         prev_ac_ir = ac_ir;
         return;
      } else {
         warmed_up = 1;
      }
   }

   static signed int32 prev2 = 0;

   if(prev_ac_ir < prev2 && prev_ac_ir < ac_ir) {
      if(ms_count - last_peak_ms > 450 && abs(prev_ac_ir) > 300) {
         beat_count++;
         last_peak_ms = ms_count;
      }
   }

   prev2 = prev_ac_ir;
   prev_ac_ir = ac_ir;

   if(abs(ac_ir) > 20) {
      ac_red_sum += (float)abs(ac_red);
      ac_ir_sum += (float)abs(ac_ir);
      sample_count++;
   }
}

// ================= ÐO =================
void start_measure() {
   start_time = ms_count;
   measuring = 1;
   finger_present = 1;
   warmed_up = 0;

   beat_count = 0;
   ac_red_sum = 0;
   ac_ir_sum = 0;
   sample_count = 0;

   prev_ac_ir = 0;
   last_peak_ms = 0;
   last_sample_ms = ms_count;
}

void finish_measure() {
   if(!finger_present) {
      bpm = 0;
      spo2 = 0;
      return;
   }

   measuring = 0;
   bpm = (float)beat_count * 6.0f;

   if(ac_ir_sum > 0 && ac_red_sum > 0 && sample_count > 200) {
      float R = (ac_red_sum / dc_red) / (ac_ir_sum / dc_ir);
      spo2 = 104.0 - 17.0 * R;

      if(spo2 > 100) spo2 = 100;
      if(spo2 < 85)  spo2 = 85;
   } else {
      bpm = 0;
      spo2 = 0;
   }
}

// ================= HX711 =================
signed int32 hx711_read(void) {
   signed int32 data = 0;
   int8 i;
   unsigned int16 timeout = 0;

   while(input(HX711_DT)) {
      delay_us(10);
      if(++timeout > 5000) return 0;
   }

   for(i = 0; i < 24; i++) {
      output_high(HX711_SCK);
      delay_us(1);
      data <<= 1;
      output_low(HX711_SCK);
      delay_us(1);
      if(input(HX711_DT)) data++;
   }

   output_high(HX711_SCK);
   delay_us(1);
   output_low(HX711_SCK);

   if(data & 0x800000)
      data |= 0xFF000000;

   return data;
}

void hx711_tare(void) {
   signed int32 val;
   float sum = 0;
   int count = 0;
   for(int i = 0; i < 5; i++) {
      val = hx711_read();
      if(val != 0) {
         sum += (float)val;
         count++;
      }
      delay_ms(10);
   }
   if(count == 0) hx711_offset = 0;
   hx711_offset = sum / count;   
}

float weight(void) {
   signed int32 val;
   float sum = 0;
   int count = 0;

   for(int i = 0; i < 5; i++) {
      val = hx711_read();
      if(val != 0) {
         sum += (float)val;
         count++;
      }
      delay_ms(10);
   }

   if(count == 0) return 0;

   float avg = sum / count;
   float net = (avg - hx711_offset) / hx711_scale;
   if(net < 0) net = 0;
   return net/1000;
}

// ================= HC-SR04 =================
float hcsr04_read_once() {
   unsigned int16 timeout;
   unsigned int32 time_us;
   output_low(TRIG);
   delay_us(2);
   output_high(TRIG);
   delay_us(10);
   output_low(TRIG);
   timeout = 0;
   while(!input(ECHO)) {
      delay_us(1);
      if(++timeout > 5000) return 0;
   }

   set_timer1(0);
   timeout = 0;
   while(input(ECHO)) {
      delay_us(1);
      if(++timeout > 5000) return 0;
   }

   time_us = get_timer1() * 1.6;
   return (float)time_us / 58.82;
}
float height() {
   float sum = 0;
   int valid = 0;

   for(int i = 0; i < 5; i++) {
      float d = hcsr04_read_once();
      if(d > 2 && d < 400) {
         sum += d;
         valid++;
      }
      delay_ms(10);
   }
   if(valid == 0) return 0;
   float hei = 20 - (sum / valid);
   if (hei < 0) hei = 0;
   return hei;
}
// ================= UART =================
void xulyUART(char c) {
   static int idx = 0;
   if(c == '\n' || c == '\r') {
      cmd[idx] = '\0';
      idx = 0;
      if(strcmp(cmd, CMD_DO) == 0) {
         lcd_putc('\f');
         lcd_gotoxy(1,1);
         printf(lcd_putc, "Dang do...");
         if(!measuring) start_measure();
      }
      else if(strcmp(cmd, CMD_LAMMOI) == 0) {
         lcd_putc('\f');
         lcd_gotoxy(1,1);
         printf(lcd_putc, "Dang reset...");
         reset_cpu();
      }
   } else {
      if(idx < sizeof(cmd) - 1)
         cmd[idx++] = c;
   }
}

// ================= PHÍM =================
void xulyphimbam() {
   lcd_putc('\f');
   lcd_gotoxy(1,1);
   printf(lcd_putc, "Dang do...");
   if(!measuring) {
      start_measure();
      request_print = 1;
   }
}

// ================= MAIN =================
void main() {  
   setup_adc_ports(NO_ANALOGS);
   setup_adc(ADC_OFF);
   set_tris_a(0);
   set_tris_b(0xF0);
   set_tris_c(0x91);
   set_tris_d(0x02);
   set_tris_e(0x00);
   port_b_pullups(TRUE);
   output_low(LED1);
   output_low(LED2);
   output_low(LED3);
   setup_timer_1(T1_INTERNAL | T1_DIV_BY_8);
   setup_timer_2(T2_DIV_BY_16, 155, 2);
   enable_interrupts(INT_TIMER2);
   enable_interrupts(GLOBAL);
   delay_ms(2000);
   hx711_tare();
   max30102_init();
   lcd_init();
   delay_ms(200);
   lcd_putc('\f');
   lcd_gotoxy(1,1);
   printf(lcd_putc,"W:%5.1f H:%5.1f", m, h);
   lcd_gotoxy(1,2);
   printf(lcd_putc,"B:%3.0f S:%3.0f%%", bpm, spo2);
   while(TRUE) {
      if(measuring) {
         if((ms_count % 500) < 250) 
         output_high(LED1);
         else 
         output_low(LED1);
         output_low(LED3);
         output_low(LED2);
         if(ms_count - last_sample_ms >= 10) {
            last_sample_ms = ms_count;
            max30102_read_fifo(&red_raw, &ir_raw);

            if(ir_raw > 45000 && !finger_present) {
               if(finger_change_ms == 0) finger_change_ms = ms_count;
               else if(ms_count - finger_change_ms > 300) {
                  finger_present = 1;
                  finger_change_ms = 0;
               }
            }
            else if(ir_raw < 35000 && finger_present) {
               if(finger_change_ms == 0) finger_change_ms = ms_count;
               else if(ms_count - finger_change_ms > 300) {
                  finger_present = 0;
                  finger_change_ms = 0;
               }
            }
            else {
               finger_change_ms = 0;
            }

            if(finger_present)
               process_data(red_raw, ir_raw);
            else {
               dc_red = (dc_red * 31 + red_raw) >> 5;
               dc_ir  = (dc_ir * 31 + ir_raw) >> 5;
            }
         }
         static int1 last_finger_state = 1;
         if(last_finger_state && !finger_present) {
            lcd_gotoxy(13,1);
            printf(lcd_putc,"PAUSE");
         }

         if(!last_finger_state && finger_present) {
            lcd_gotoxy(13,1);
            printf(lcd_putc,"     ");
            warmed_up = 0;
            start_time = ms_count;
            prev_ac_ir = 0;
            last_peak_ms = 0;
         }

         last_finger_state = finger_present;

         if(finger_present && (ms_count - start_time >= 10000)) {
            finish_measure();
            request_print = 1;
         }
      }

      if(kbhit()) {
         char c = getc();
         xulyUART(c);
      }
      if(!measuring && request_print) {
         h = height();
         m = weight();
         output_low(LED1);
         if(m <= 0.01 || h <= 0.01 || m > 1)
         {
         output_high(LED2);
         bmi = 0; // không tính
         }
         else
         {
         output_high(LED3);
         float hm = h / 100.0;
         bmi = m / (hm * hm);
         }
         lcd_gotoxy(1,1);
         printf(lcd_putc,"W:%5.3f H:%5.1f", m, h);
         lcd_gotoxy(1,2);
         printf(lcd_putc,"B:%3.0f S:%3.0f%%", bpm, spo2);
         printf("W: %5.1f\r\n", m);
         delay_ms(50);
         printf("H: %5.1f\r\n", h);
         delay_ms(50);
         printf("B: %3.0f\r\n", bpm);
         delay_ms(50);
         printf("S: %3.0f\r\n", spo2); 
         delay_ms(50);
         printf("BMI: %5.1f\r\n", bmi);
         request_print = 0;
      }
      key = keypad_getkey();
      if(key == 'B')
         xulyphimbam();
   }
}

