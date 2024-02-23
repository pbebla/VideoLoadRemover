# VideoLoadRemover

How to Use:

Open Livesplit, set timing method to Game Time, that's it. Now to setup the load remover.

1. Unzip and launch the .exe
![image](https://github.com/pbebla/VideoLoadRemover/assets/26189864/f4d2460b-30c9-41b6-b1df-288d0b7dcd65)

2. Load Reference Image: File->Load Image
![image](https://github.com/pbebla/VideoLoadRemover/assets/26189864/dd9c49d5-9ced-4626-aa7a-be7a4b10ba1e)

3. Choose Window to Capture: Select from dropdown menu.
![image](https://github.com/pbebla/VideoLoadRemover/assets/26189864/a1b4e9b3-5ee3-4f3f-96e7-4925b1371c6d)

4. Create Capture Region: Click and drag on left window screen to create capture region. What's covered by the green rectangle will show in the right box. You can also adjust using the X,Y,Width,and Height parameters.
You will see a number display on the right side. That is the Peak Signal to Noise Ratio number (PSNR) calculated against the reference image and used to determine load frames. (Preview image not filling up entire area of the right box is ok. The green rectangle might not match up perfectly with the preview box, but it's what's shown in the right box that ulimately matters.)
![image](https://github.com/pbebla/VideoLoadRemover/assets/26189864/1aaa8d1f-4d75-44b4-b64c-93f8da03db2b)

You can toggle the reference image to overlay preview region by clicking "Show Diff". Try to match up the two images as much as possible, then note down the PSNR value.
(Note: I recommend toggling off the diff display once it is setup.)
![image](https://github.com/pbebla/VideoLoadRemover/assets/26189864/38cfa57b-f56f-43f9-8828-03758fe555e4)

5. Set a PSNR threshold value
After noting down PSNR value of the load frame, determine a threshold value. There's no exact science to determine this, but in this case I'll set it to 19.
![image](https://github.com/pbebla/VideoLoadRemover/assets/26189864/45433660-2a99-4edc-b861-755f7e641850)

6. Start the Load Remover.
Before starting the Livesplit timer, click "Start" on the load remover to begin.





