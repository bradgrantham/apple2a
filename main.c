char *title = "Apple IIa";
unsigned char title_length = 9;

unsigned char *text_page1_base = (unsigned char *)0x400;

int main(void)
{
    int offset = (80 - title_length) / 2;

    unsigned char *loc = text_page1_base + offset;

    int i;

    for(i = 0; i < 80 * 24; i++)
        text_page1_base[i] = ' ' | 0x80;

    for(i = 0; i < title_length; i++)
        loc[i] = title[i] | 0x80;

    while(1);
}
