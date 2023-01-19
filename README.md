# Godspeeding towards the best ILP formulation for Sudoku

Please refer: https://github.com/coin-or/Cbc/discussions/516

I have been able to generate the files as suggested in the above comment. I followed the following steps and developed the folders for the same:-

1) Used Gurobi to create solved Sudokus (all squares completely filled)

2) Filtered the Sudoku to get rid of some random values from squares, with a certain probability (Obtained 3 sets of Output files as Sudoku sample problems)

3) Created base files to solve these Sets using my new method of replacing each Sudoku value (x) with (2^(x-1))-1 and equating them to unique sums. The variables in this case would be Bucket/Lotsize {This is being Modelled in two ways: Once when the Lotsize variables are initially formulated as Binary and the Next when the Lotsize variables are formulated as Integer variables} [Rounding off errors are present from Size 8 onwards and this has been reported to Gurobi Staff alongwih a request for introducing Bucket variables for testing: https://support.gurobi.com/hc/en-us/community/posts/9160893511185-Requesting-a-new-type-of-variable-introduction. As per their replies, this seems to be a case for the finite precision grinch. In a 64-bit system, a register can hold a number up to 1.8*1e19 (unsigned). This means that our number is simply too large for standard precision used in modern computers. I was strongly recommended to rescale the models and work within the comfortable limits of the computer architecture. 64-bit double-precision floating point representations allow for 16 base-10 digits of accuracy and our number being 20 digits long, roundoff error is to be expected when storing this number as a double-precision float. Additionally, the number is many orders of magnitude larger than recommended for a numerically stable model: https://www.gurobi.com/documentation/10.0/refman/recommended_ranges_for_var.html]

4) Created base files to solve the sudoku Sets using the original method of using Binary variables with layers for each of the values.



The input to the CBC would be the .LOT and .LP file of the same Sudoku Sizes from the Base Files in 3). (.MPS files are also generated in case it is necessary)

The difference in solution times between the general Sudoku ILP with Binary Variables and my approach with Lotsize variables seems considerable as per your latest reporting. I am presently trying to understand how to theoretically find the computational complexity of Linear programs and compare my Lotsized Formulation with the Original Binary ILP for Sudoku (as available in 4)).

I needed proof that this method is superior to the existing ILP with binary variables. However I would be starting a report for solving Sudoku with Lotsize/Bucket variables and would be eager to have you collaborate on that. I should be able to finish it partly within the next month. I feel there should be some theoretical proof that this should be faster.

Motivation: I was highly inspired by this Youtube video (https://www.youtube.com/watch?v=YX40hbAHx3s) and understood that a faster Sudoku algorithm would help in Cancer research. I therfore have been trying to create a solution approach to Sudoku and using bucket variables flashed to me more than a year ago. I have been able to theoretically understand that the search space in my formulation would be much less. But computational proof has been elusive to me since for this I needed a Bucket/Lotsize variable and I am yet to recieve help from anywhere in this regard. (I had approached CBC maintainers multiple times as well as Gurobi staff.)

Your help would possibly develop newer research avenues into lotsize/Bucket variables as an extension of the Binary.

Thanking you in anticipation, Yours Faithfully

Santanu Banerjee (https://www.linkedin.com/in/santanu-banerjee-093929150/)

PS.:-
I initially hunched that the Sudoku numbers is replaced with the Fibonacci Sequence would be able to solve it faster. 
However later I developed this: [https://oeis.org/A349777] thinking that the Conway Guy Sequence would be the actual deal which was also incorrect.
Ultimately the powers of 2 starting from 0, 1, 3, 7... is found to be the actual (possible) extension for Binary Variables...


FYI: To solve Sudoku using a QC :: https://github.com/dWave-examples/Sudoku
