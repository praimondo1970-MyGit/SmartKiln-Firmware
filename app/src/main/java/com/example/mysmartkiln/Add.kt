package com.example.mysmartkiln

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.mysmartkiln.ui.theme.AppColors

@Composable
fun Add(modifier: Modifier = Modifier) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .background(color = AppColors.color_Neutral_50)
    ) {
        // Main content
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .padding(24.dp),
            contentAlignment = Alignment.Center
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Plus icon
                Image(
                    painter = painterResource(id = R.drawable.plusicon),
                    contentDescription = "Add",
                    modifier = Modifier
                        .size(64.dp)
                        .padding(bottom = 16.dp)
                )
                
                // Title
                Text(
                    text = "Add New",
                    fontSize = 24.sp,
                    fontWeight = FontWeight.Bold,
                    color = AppColors.color_Neutral_900,
                    textAlign = TextAlign.Center
                )
                
                // Subtitle
                Text(
                    text = "Create a new firing program",
                    fontSize = 16.sp,
                    color = AppColors.color_Neutral_600,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.padding(top = 8.dp)
                )
            }
        }
        
        // Bottom navigation bar
        Union()
    }
}

@Composable
fun Union(modifier: Modifier = Modifier) {
    BottomAppBar(
        containerColor = AppColors.color_Primary_600,
        modifier = modifier
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceEvenly,
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Kilns icon
            IconButton(
                onClick = { /* TODO: Navigate to kilns */ }
            ) {
                Image(
                    painter = painterResource(id = R.drawable.typespeedometersize24colorblack),
                    contentDescription = "Kilns",
                    modifier = Modifier.size(24.dp)
                )
            }
            
            // Programs icon
            IconButton(
                onClick = { /* TODO: Navigate to programs */ }
            ) {
                Image(
                    painter = painterResource(id = R.drawable.typeprogramssize24colorblack),
                    contentDescription = "Programs",
                    modifier = Modifier.size(24.dp)
                )
            }
            
            // Add button (center)
            FloatingActionButton(
                onClick = { /* TODO: Add new item */ },
                containerColor = AppColors.color_Primary_200,
                modifier = Modifier.size(56.dp)
            ) {
                Image(
                    painter = painterResource(id = R.drawable.plusicon),
                    contentDescription = "Add",
                    modifier = Modifier.size(24.dp)
                )
            }
            
            // Activity icon
            IconButton(
                onClick = { /* TODO: Navigate to activity */ }
            ) {
                Image(
                    painter = painterResource(id = R.drawable.typeactivitysize24colorblack),
                    contentDescription = "Activity",
                    modifier = Modifier.size(24.dp)
                )
            }
            
            // Settings icon
            IconButton(
                onClick = { /* TODO: Navigate to settings */ }
            ) {
                Image(
                    painter = painterResource(id = R.drawable.typesettingssize24colorblack),
                    contentDescription = "Settings",
                    modifier = Modifier.size(24.dp)
                )
            }
        }
    }
}

@Preview(widthDp = 390, heightDp = 844)
@Composable
private fun AddPreview() {
    Add(Modifier)
}












