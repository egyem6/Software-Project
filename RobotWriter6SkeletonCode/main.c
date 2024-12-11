#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */
#define Max_Characters 128          //128 ASCII characers
#define Max_Width 100               //Maximum 100mm width 

//Strucutre to store font data for each ASCII value
typedef struct
{
    int ASCIICode;
    int numStrokes;
    int (*strokeData)[3];
}  CharacterData;

//Function declerations
void SendCommands (char *buffer );
double getTextHeight(); //Prompts user to input a height for the text between 4-10mm
double calculateScaleFactor(double textHeight); //Calculates the scale factor based on the text height
CharacterData* loadFontData(const char *filename, int *numCharacters); //Organises font data
void processText(const char *filename, CharacterData *fontArray, int numCharacters, double scaleFactor, double textHeight); //Processes text file
//NOTE: The function to process the text file must be altered when changing between testing on the robot and testing on the emulator. 
//The section within the function dedicated to the generation of G code must be altered, the G0/G1 outputs need to change and the SendCommands function needs to be uncommented. 
//The changes are further discussed within the function...

void OutputToTerminal(char *buffer);
void freeMemory(CharacterData *fontArray, int numCharacters);

int main()
{
    //Varible definition
    double textHeight,scaleFactor; 
    int numCharacters=0;

    //Name of font file
    const char *fontFile="SingleStrokeFont.txt"; 

    //Name of text file to be written by robot (alter name as neccesary)
    const char *textfile="test.txt"; 

    //Load font data into system
    CharacterData *fontArray=loadFontData(fontFile, &numCharacters);
    if (!fontArray)
    {
        return 1; //If font loading fails, exit 
    }
    printf("Loaded %d characters from font file. \n", numCharacters);
    
    //char mode[]= {'8','N','1',0};
    char buffer[100];

    //Calls getTextHeight function
    textHeight=getTextHeight();

    //Calls calculateScaleFactor function
    scaleFactor=calculateScaleFactor(textHeight);
    printf("Scale Factor: %.4f\n", scaleFactor);

    

    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n"); //When running for the emulator, the user must hit enter 3 times at this point before the Gcode is output to the terminal
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);

    

    //Call processTextFileTest function
    processText(textfile, fontArray, numCharacters, scaleFactor, textHeight);

    //Put pen in the 0,0 position with pen up
    sprintf (buffer, "G0 X0 Y0\n"); //Replace S0 with G0 during testing

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    //Free allocated memory before exiting
    freeMemory(fontArray, numCharacters);


    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}


//Function Definitions
// Function to prompt and validate text height input
double getTextHeight()
{
    double textHeight;
    int valid=0;

    while (!valid) 
    {
        printf("Enter text height in mm (between 4 and 10 mm): "); //Prompts user for an input
        if (scanf("%lf", &textHeight)!=1) 
        {
            printf("Invalid input. Please enter a numeric value.\n");
            while (getchar()!='\n'); 
        } else if (textHeight<4||textHeight>10) //Validates input is within 4-10mm range
        {
            printf("Invalid text height. It must be between 4 and 10 mm.\n");
        } else 
        {
            valid=1; // Input is valid
        }
    }
    return textHeight;
}
// Function to calculate the scale factor
double calculateScaleFactor(double textHeight) 
{
    return textHeight/18.0;
}

// Function to load font data from a file
CharacterData* loadFontData(const char *filename, int *numCharacters) 
{
    FILE *file = fopen(filename, "r"); //Check if font data file opened corectly
    if (!file) 
    {
        printf("Error: Could not open font file %s\n", filename);
        return NULL;
    }

    CharacterData *fontArray=malloc(Max_Characters*sizeof(CharacterData)); //Check if memorey allocation for fontarray was correct
    if (!fontArray) 
    {
        printf("Error: Memory allocation failed.\n");
        fclose(file);
        return NULL;
    }

    *numCharacters=0; // Initialize character count
    while (!feof(file)) 
    {
        int marker, ASCIICode, numStrokes;

        // Read marker, ASCIICode, and numStrokes
        if (fscanf(file, "%d %d %d", &marker, &ASCIICode, &numStrokes)!= 3||marker!=999) 
        {
            break;
        }

        CharacterData *currentChar=&fontArray[*numCharacters];
        currentChar->ASCIICode=ASCIICode;
        currentChar->numStrokes=numStrokes;
        currentChar->strokeData=malloc(numStrokes*sizeof(int[3]));
        if (!currentChar->strokeData) //Check if memory allocation for stroke data was successfull
        {
            printf("Error: Memory allocation for strokes failed.\n");
            fclose(file);
            return NULL;
        }

        // Read strokes
        for (int i=0; i<numStrokes; i++) 
        {
            fscanf(file, "%d %d %d", &currentChar->strokeData[i][0],
                   &currentChar->strokeData[i][1],
                   &currentChar->strokeData[i][2]);
        }

        (*numCharacters)++;
    }

    fclose(file);
    return fontArray;
}
//Function to process the text file
void processText(const char *filename, CharacterData *fontArray, int numCharacters, double scaleFactor, double textHeight) 
{
    //Check if file opened corectly
    FILE *file=fopen(filename, "r");
    if (!file) 
    {
        printf("Error: Could not open text file %s\n", filename); 
        return;
    }

    char word[100];   // Buffer for each word
    double xOffset=0; // Horizontal position
    double yOffset=0; // Vertical position
    double Line_Spacing=textHeight+5.0;
    int c;            // Character read from the file

    while ((c=fgetc(file))!=EOF) 
    {
        // Handle Line Feed (LF) and Carriage Return (CR)
        if (c==10) 
        { // LF (ASCII 10)
            xOffset=0;               // Reset horizontal offset
            yOffset-=Line_Spacing;   // Move to the next line
            printf("Line Feed: Moving to next line at Y offset %.2f\n", yOffset);
            continue;
        } 
        else if (c==13) 
        { // CR (ASCII 13)
            xOffset=0; // Reset horizontal offset
            printf("Carriage Return: Resetting X offset to %.2f\n", xOffset);
            continue;
        }

        // Push back the character if it's part of a word
        ungetc(c, file);

        // Read the word
        if (fscanf(file, "%99s", word)!=1) 
        {
            break;
        }

        printf("Processing word: %s\n", word);

        // Process each letter in the word
        double wordWidth=0.0;
        for (int i=0; word[i]!='\0'; i++) 
        {
            CharacterData *charData=NULL;
            for (int j=0; j<numCharacters; j++) 
            {
                if (fontArray[j].ASCIICode==word[i]) 
                {
                    charData=&fontArray[j];
                    break;
                }
            }

            if (charData) 
            {
                wordWidth+=18.0*scaleFactor; 
            }
        }
        wordWidth+=18.0*scaleFactor; // Add spacing for the word

        // Check if the word fits in the current line
        if (xOffset+wordWidth>Max_Width)
        {
            xOffset=0;               // Reset horizontal position
            yOffset-=Line_Spacing;   // Move to the next line
            printf("Word exceeds line width. Moving to next line at Y offset %.2f\n", yOffset);
        }

        // Process each letter in the word
        for (int i=0; word[i]!='\0'; i++) 
        {
            CharacterData *charData = NULL;
            for (int j=0; j<numCharacters; j++) 
            {
                if (fontArray[j].ASCIICode==word[i]) 
                {
                    charData=&fontArray[j];
                    break;
                }
            }

            if (charData) 
            {
                // Generate G-code for each stroke
                for (int k=0; k<charData->numStrokes; k++) 
                {
                    int x=charData->strokeData[k][0];
                    int y=charData->strokeData[k][1];
                    int pen=charData->strokeData[k][2];

                    double scaledX=xOffset+x*scaleFactor;
                    double scaledY=yOffset+y*scaleFactor;

                    char buffer[100];
                    if (pen==0) 
                    { // Pen up, Replace G0 with S0 during robot testing
                        sprintf(buffer, "G0 X%.2f Y%.2f\n", scaledX, scaledY);
                    } else 
                    { // Pen down, Replace G1 with S1000 during robot testing
                        sprintf(buffer, "G1 X%.2f Y%.2f\n", scaledX, scaledY);
                    }
                    OutputToTerminal(buffer); // Output G-code to terminal

                    //Send G-code commands 
                    //SendCommands(buffer); //This sends the G code commands to the robot, must be uncommented out for robot testing. 
                    //When using the emulatorit can be left as a comment.
                }

                // Update xOffset
                xOffset+=20.0*scaleFactor; // Increment horizontal position
            }
        }

        // Add space after the word
        xOffset+=5.0*scaleFactor;
    }

    fclose(file);
}

//Function to output G-code to terminal window
void OutputToTerminal(char *buffer)
{
    printf("%s", buffer);
}

//Function to free allocated memory
void freeMemory(CharacterData *fontArray, int numCharacters) 
{
    if (!fontArray) 
    {
        return; // If fontArray is NULL, no need to free
    }

    // Free each strokeData array
    for (int i = 0; i < numCharacters; i++) 
    {
        if (fontArray[i].strokeData) 
        {
            free(fontArray[i].strokeData);
        }
    }

    // Free the fontArray itself
    free(fontArray);
    printf("Memory for font data has been successfully freed.\n");
}


