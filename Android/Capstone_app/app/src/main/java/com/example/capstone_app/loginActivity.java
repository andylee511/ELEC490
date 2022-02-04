package com.example.capstone_app;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

public class loginActivity extends AppCompatActivity {

    //creating objects
    EditText user_name, password;
    Button signin;
    DBHelper DB;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_login);
        user_name = findViewById(R.id.user_nameS); //username
        password = findViewById(R.id.passwordS);// password
        signin = findViewById(R.id.signin); //sign in  button
        DB = new DBHelper(this);


        signin.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View view) {
                String user = user_name.getText().toString();
                String pass = password.getText().toString();

                if (TextUtils.isEmpty(user) || TextUtils.isEmpty(pass)) {
                    Toast.makeText(loginActivity.this, "All Fields Required", Toast.LENGTH_SHORT).show();
                } else //login field is not empty
                {
                    Boolean checkuserpass = DB.checkusernamepassword(user, pass);
                    if (checkuserpass == true)//user exists in the database
                    {
                        Toast.makeText(loginActivity.this, "Sign in successfully ", Toast.LENGTH_SHORT).show();
                        Intent intent = new Intent(getApplicationContext(), HomeActivity.class);
                        startActivity(intent); //go to the homeactivity
                    } else { //user not exits in the database
                        Toast.makeText(loginActivity.this, "Password or Username Incorrect", Toast.LENGTH_SHORT).show();
                    }
                }


            }
        });

    }
}