package com.example.capstone_app;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {
    EditText user_name, password,confirm_password;
    Button register,had_account;
    DBHelper DB;




    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        user_name=findViewById(R.id.user_name); //username
        password=findViewById(R.id.password);// password
        confirm_password=findViewById(R.id.confirm_password); //confirm _password
        register=findViewById(R.id.register); //sign up  button
        had_account=findViewById(R.id.had_account); //sign in button
        DB=new DBHelper(this);


        register.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String user=user_name.getText().toString();
                String pass=password.getText().toString();
                String repass=confirm_password.getText().toString();
                if(TextUtils.isEmpty(user)||TextUtils.isEmpty(pass)||TextUtils.isEmpty(repass))
                    Toast.makeText(MainActivity.this,"All Fields Required",Toast.LENGTH_SHORT).show();
                else{
                    if( pass.equals(repass)) //if the fields entered is not empty and password entered is equal to the password confirmation
                    {
                        Boolean checkuser=DB.checkusername(user);
                        if(checkuser==false)//user is not stored in the database
                        {
                            Boolean insert=DB.insertData(user,pass); //if this user has not been registered, then add this user
                            if(insert==true){
                                Toast.makeText(MainActivity.this, "Register Successfully", Toast.LENGTH_SHORT).show();
                                Intent intent=new Intent(getApplicationContext(), HomeActivity.class);
                                startActivity(intent);
                            }
                            else{
                                Toast.makeText(MainActivity.this,"Registration Failed",Toast.LENGTH_SHORT).show();//cant register
                            }
                        }
                        else //user is stored in the database
                        {
                            Toast.makeText(MainActivity.this,"User already registers",Toast.LENGTH_SHORT).show();//cant register
                        }
                    }
                    else
                    {
                        Toast.makeText(MainActivity.this,"Password does not match",Toast.LENGTH_SHORT).show();//cant register
                    }
                }


            }
        });

        had_account.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {//when already had acccount button got clicked, we go to the login page
                Intent hadaccount_intent=new Intent(getApplicationContext(),loginActivity.class);
                startActivity(hadaccount_intent);//go to the login page




            }
        });



    }
}