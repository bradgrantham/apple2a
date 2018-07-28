char *title = "Apple IIa";
unsigned char title_length = 9;

volatile unsigned char *text_page1_base = (unsigned char *)0x400;
volatile unsigned char *text_page2_base = (unsigned char *)0x800;

int main(void)
{
    int offset = (40 - title_length) / 2;

    volatile unsigned char *loc = text_page1_base + offset;

    int i;

    for(i = 0; i < (40 + 40 + 65) * 8; i++) {
        text_page1_base[i] = ' ' | 0x80;
    }

    for(i = 0; i < title_length; i++) {
        loc[i] = title[i] | 0x80;
    }

    while(1);
}
