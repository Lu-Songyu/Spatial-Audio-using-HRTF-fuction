# SRIP_Hrtf_Spatial_Audio_Project 
<h2>Background</h2>

This program is part of Summer Research Internship Program by Eletrical Computing Engineering Department at UC-San Diego.


<h2>Abstract</h2>
As of now, there are not many online tools available to learn about spatial audio, and almost no demos that experiment with personalization of head-related transfer function (HRTF). HRTF describes how a sound wave is affected by the head and body as it travels through space, so personalizing it to an individual’s anthropometric measurements can improve sound localization. The CIPIC database contains ear measurements and corresponding HRTFs of 45 people that can be used by researchers to find an individual’s closest HRTF match. Previous HRTF matching techniques have extracted distances from ear pictures, relying mostly on ear size. Our Matlab application offers three algorithms that provide a match based on ear shape. The user can select from block segmentation with Hu moment invariants, principal component analysis, and Q-vector analysis. After the closest ear shape match from the CIPIC database is identified, the corresponding HRTF is used in our demo (instead of the standard MIT KEMAR model HRTF). The demo was created by building on a Github program of a sound moving 360 degrees horizontally. We used C language with Simple DirectMedia Layer 2 libraries, creating a layout for the user to specify the azimuth of the sound that they are hearing. We are creating an easily accessible, streamlined educational module for users to learn about spatial audio, and test for themselves whether the localization of the audio is improved. <br>
<a href="https://sol0092.wixsite.com/website" target="_blank">Instruction: setup</a> <br> </p> 

<h2>Team</h2>
Academic Advisor: Professor Truong Nguyen <br>

<table style="width: 100%;">
  <caption style="text-align:right">Contact Information</caption>
  <tr>
    <th colspan="2"><b>Name</b></td>
    <th><b>Email</b></td>
  </tr>
  <tbody>
    <tr>
      <td>Songyu</td>
      <td>Lu</td>
      <td>sol009@ucsd.edu</td>
    </tr>
  </tbody>
  <tbody>
    <tr>
      <td>Branson</td>
      <td>Beihl</td>
      <td>bbeihl@ucsd.edu</td>
    </tr>
  </tbody>
</table>

<h2>Instruction</h2>
<strong>Please set up your environment first</strong>
1. compile: <code>gcc -I deps/kiss_fft130 hrtf.c deps/kiss_fft130/kiss_fft.c -lmingw32 -lSDL2main  -llibSDL2</code> <br>
2. run: <code>./a.exe</code> 

<br>
<br>

Credit to Ryan Huffman's hrtf-spatial-audio project, which this project was based upon. 
